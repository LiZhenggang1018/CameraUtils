# src/LivePhotoSmoothPlay —— 逐文件实现细节

本文档面向**实现维护者**，详细说明 `ImageRegistrator.cpp` 和 `YUVImageReader.cpp` 的实现细节、设计决策、历史修复。
模块总览、数据流、性能分析、调试方法请见同目录的 [`README.md`](./README.md)。

---

## 目录

- [1. ImageRegistrator.cpp —— 配准算法](#1-imageregistratorcpp--配准算法)
- [2. YUVImageReader.cpp —— YUV 读写](#2-yuvimagereadercpp--yuv-读写)
- [3. 历史修复汇总](#3-历史修复汇总)

---

## 1. ImageRegistrator.cpp —— 配准算法

### 文件内部结构

```
createDetector          → 工厂：ORB/BRISK/KAZE/AKAZE
detectFeatures          → BGR/BGRA/GRAY → GRAY → detectAndCompute
matchFeatures           → BFMatcher/FLANN + knnMatch + Lowe's ratio
estimateTransform       → RANSAC 估计 H/仿射/欧氏/平移
applyTransform          → warpPerspective/warpAffine
registerImages          → 主入口：检测 → 匹配 → 估计 → 求逆 → warp → RMS
drawMatches             → 绘制匹配
saveRegistrationResults → 保存 3 个文件
```

### 1.1 `createDetector` —— ORB 用 5000 个特征点

```cpp
case FeatureDetectorType::ORB:
    return cv::ORB::create(5000);   // ← 比默认 500 多 10 倍
```

**设计意图**：视频帧和封面帧的 FOV 差异约 6%，默认 500 个点在 RANSAC 后内点不足。5000 个点能保证"匹配点 ~1500、内点 ~900"——足够稳定。

**代价**：ORB 检测从 ~5ms 涨到 ~30~50ms/帧。但**配准只做一次**，不影响批量处理。

### 1.2 `detectFeatures` —— 兼容 BGR/BGRA/GRAY

```cpp
cv::Mat gray;
if (image.channels() == 1) {
    gray = image;
} else if (image.channels() == 3) {
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
} else if (image.channels() == 4) {
    cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
} else {
    throw std::runtime_error("不支持的图像通道数: " + std::to_string(image.channels()));
}
```

### 1.3 `matchFeatures` —— BFMatcher 与 FLANN 分派

```cpp
if (detector_type == ORB || detector_type == BRISK) {
    matcher = cv::BFMatcher::create(cv::NORM_HAMMING);   // 二进制描述符
} else {
    matcher = cv::BFMatcher::create(cv::NORM_L2);        // 浮点描述符
    use_flann = true;
}

// 浮点描述符尝试 FLANN
if (use_flann && ...) {
    try {
        cv::Ptr<cv::flann::IndexParams> indexParams;
        if (descriptors1.type() == CV_32F)
            indexParams = cv::makePtr<cv::flann::KDTreeIndexParams>(5);
        else
            indexParams = cv::makePtr<cv::flann::LshIndexParams>(20, 10, 2);
        ...
        matcher = cv::makePtr<cv::FlannBasedMatcher>(indexParams, searchParams);
    } catch (...) {
        matcher = cv::BFMatcher::create(cv::NORM_L2);   // 回退
    }
}
```

**关键**：
- 二进制描述符必须用 `NORM_HAMMING`（ORB/BRISK/AKAZE 默认是二进制的）
- 浮点描述符用 `NORM_L2`（KAZE 是浮点的）
- FLANN 在某些 OpenCV 构建下不可用——fallback 到 BFMatcher 保证功能

### 1.4 `estimateTransform` —— RANSAC 配置 + 空 mask 检查

```cpp
case HOMOGRAPHY:
    transform = cv::findHomography(points1, points2, cv::RANSAC, 3.0, inliers_mask, 2000, 0.995);
```

| 参数 | 值 | 语义 |
| :--- | :---: | :--- |
| `method` | `RANSAC` | 随机采样一致性 |
| `ransacReprojThreshold` | `3.0` | 重投影误差 < 3 像素算内点 |
| `maxIters` | `2000` | 最大迭代次数 |
| `confidence` | `0.995` | 找到正确解的概率 |


**其他变换类型**：

```cpp
case AFFINE:
    transform = cv::estimateAffine2D(points1, points2, inliers_mask, cv::RANSAC, 3.0, 2000, 0.995);
    // 补 3x3 矩阵
    cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
    transform.copyTo(h.rowRange(0, 2));
    h.at<double>(2, 2) = 1.0;
    transform = h;
    break;

case TRANSLATION:
    // 用中位数作为平移量，不做 RANSAC
    ...
    transform = (cv::Mat_<double>(3, 3) << 1, 0, tx, 0, 1, ty, 0, 0, 1);
    inliers_mask = std::vector<uchar>(points1.size(), 1);
    break;
```

### 1.5 `registerImages` —— 逆矩阵处理（关键设计）

```cpp
// estimateTransform 返回的是 reference → target 的矩阵（points1 → points2）
result.transform_matrix = estimateTransform(keypoints1, keypoints2, ...);

// 为了把 target 对齐到 reference，需要逆矩阵
cv::Mat warp_to_ref;
cv::invert(result.transform_matrix, warp_to_ref, cv::DECOMP_SVD);

// 应用到 target_img
result.registered_image = applyTransform(target_img, warp_to_ref, reference_img.size());
```

**为什么必须求逆**：

- `estimateTransform` 内部 `findHomography(points1, points2, ...)` 返回的 H 满足 `points2 = H * points1`
- `warpPerspective(image, dst, H, ...)` 内部实际做的是 `dst(x) = image(H^-1 * x)`
- 要把 `target_img` 对齐到 `reference_img` 的坐标系，需要 `points1 = H^-1 * points2`，即 warp 用 `H^-1`

**`cv::DECOMP_SVD`** 是数值最稳的求逆方法——对接近奇异的矩阵也能给出合理结果。

### 1.6 RMS 误差计算

```cpp
for (const auto &match : inlier_matches) {
    cv::Point2f pt_ref = keypoints1[match.queryIdx].pt;
    cv::Point2f pt_tar = keypoints2[match.trainIdx].pt;

    cv::Mat pt_tar_homog = (cv::Mat_<double>(3, 1) << pt_tar.x, pt_tar.y, 1);
    cv::Mat pt_tar_transformed = warp_to_ref * pt_tar_homog;
    pt_tar_transformed /= pt_tar_transformed.at<double>(2, 0);   // 齐次归一化

    double dx = pt_ref.x - pt_tar_transformed.at<double>(0, 0);
    double dy = pt_ref.y - pt_tar_transformed.at<double>(1, 0);
    result.rms_error += dx * dx + dy * dy;
}
result.rms_error = std::sqrt(result.rms_error / inlier_matches.size());
```

**方向正确**：把 target 点用 `warp_to_ref` 映射回 reference 坐标系，与 reference 点比较。

**只在 `inlier_matches` 上算**——不是全部 `good_matches`，避免误匹配拉高 RMS。

---

## 2. YUVImageReader.cpp —— YUV 读写

### 文件内部结构

```
构造函数              → 计算 y_size_ / uv_size_
readYUV420P           → YUV 文件 → BGR Mat（limited-range）
readYUV420PChannels   → YUV 文件 → 3 个独立 cv::Mat（深拷贝）
convertBGRtoYUV420P   → BGR → YUV 缓冲区（limited-range）
saveAsYUV420P         → BGR → YUV 文件
```

### 2.1 构造函数

```cpp
YUVImageReader::YUVImageReader(int width, int height)
    : width_(width), height_(height)
{
    y_size_  = width * height;
    uv_size_ = (width / 2) * (height / 2);
}
```

**实例是"尺寸绑定"的**——一个实例只能读/写 `width × height` 的 YUV。

### 2.2 `readYUV420P` —— 严格校验 + I420 转换

```cpp
std::ifstream file(filepath, std::ios::binary | std::ios::ate);
std::streamsize file_size = file.tellg();
file.seekg(0, std::ios::beg);

const size_t expected_size = static_cast<size_t>(y_size_ + 2 * uv_size_);
if (static_cast<size_t>(file_size) != expected_size) {
    throw std::runtime_error("文件大小与YUV420P格式不匹配");
}
```

**严格校验**——必须精确 `y_size_ + 2 * uv_size_` 字节。

### 保留的已知项

| 位置 | 说明 | 影响 |
| :--- | :--- | :--- |
| `LpReg.h::DecodeCropEncodeInputVid` | `1562` / `1260` / `300` 硬编码对应 X300U 录屏工具 | 主流程不调用，仅"录屏 LP 预处理"用；换录屏时必须手动改 |
| `YUVProcessor::ConvertCoverFrameToMultiFovYuvs` | 旧版 FOV 生成（1920×1080 硬编码） | 死代码，无调用 |
| `DecMp4ToYuvFrames` 的 `jpeg_sws_ctx.release()` | `release()` 后手动 `sws_freeContext` | 逻辑安全（不漏不多），只是风格冗余 |
| `ImageRegistrator` 硬编码参数 | ORB 5000 / RANSAC 3.0 / ratio 0.75 | 不同场景不能调，但当前场景够用 |

---

## 相关文档

- **模块总览**：[`README.md`](./README.md) —— 文件结构、数据流、性能分析、调试方法
- **接口说明**：[`../../inc/LivePhotoSmoothPlay/README.md`](../../inc/LivePhotoSmoothPlay/README.md) —— 每个类的公开 API、使用示例、注意事项
- **测试说明**：[`../../tests/LivePhotoSmoothPlay/README.md`](../../tests/LivePhotoSmoothPlay/README.md) —— 测试入口、依赖数据、运行方式