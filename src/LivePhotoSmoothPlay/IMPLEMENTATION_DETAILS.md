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
detectFeatures          → BGR → GRAY → detectAndCompute
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

### 1.2 `matchFeatures` —— BFMatcher 与 FLANN 分派

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
- **二进制描述符必须用 `NORM_HAMMING`**（ORB/BRISK/AKAZE 默认是二进制的）
- **浮点描述符用 `NORM_L2`**（KAZE 是浮点的）
- **FLANN 在某些 OpenCV 构建下不可用**——fallback 到 BFMatcher 保证功能

### 1.3 `estimateTransform` —— RANSAC 配置

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

**3.0 像素阈值**对 720×1584 是"~0.4% 画幅"——**比较严格**，能过滤掉误匹配但要求初始匹配质量高。

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
    std::vector<float> dx, dy;
    for (size_t i = 0; i < points1.size(); ++i) {
        dx.push_back(points2[i].x - points1[i].x);
        dy.push_back(points2[i].y - points1[i].y);
    }
    std::nth_element(dx.begin(), dx.begin() + dx.size() / 2, dx.end());
    std::nth_element(dy.begin(), dy.begin() + dy.size() / 2, dy.end());
    float tx = dx[dx.size() / 2];
    float ty = dy[dy.size() / 2];
    transform = (cv::Mat_<double>(3, 3) << 1, 0, tx, 0, 1, ty, 0, 0, 1);
    inliers_mask = std::vector<uchar>(points1.size(), 1);   // 全部内点
    break;
```

**`AFFINE` / `EUCLIDEAN` 的 3×3 补齐**：`estimateAffine2D` 返回 2×3，补成 3×3 方便后续 `applyTransform` 统一处理。

**`TRANSLATION` 的全内点**：不做 RANSAC，所有匹配点都算内点，`num_inliers` 会虚高。当前不用，影响不大。

### 1.4 `registerImages` —— 逆矩阵处理（关键设计）

```cpp
// estimateTransform 返回的是 reference → target 的矩阵（points1 → points2）
result.transform_matrix = estimateTransform(keypoints1, keypoints2, ...);

// 为了把 target 对齐到 reference，需要逆矩阵
cv::Mat warp_to_ref;
cv::invert(result.transform_matrix, warp_to_ref, cv::DECOMP_SVD);

// 应用到 target_img
result.registered_image = applyTransform(target_img, warp_to_ref, reference_img.size());
```

**注释写得很清楚**：

> 估计得到的矩阵是从 reference（视频帧）→ target（封面帧）。我们为了把 target（封面帧）对齐到 reference（视频帧），需要使用该矩阵的逆。

**为什么必须求逆**：

- `estimateTransform` 内部 `findHomography(points1, points2, ...)` 返回的 H 满足 `points2 = H * points1`
- `warpPerspective(image, dst, H, ...)` 内部实际做的是 `dst(x) = image(H^-1 * x)`
- 要把 `target_img` 对齐到 `reference_img` 的坐标系，需要 `points1 = H^-1 * points2`，即 warp 用 `H^-1`

**`cv::DECOMP_SVD`** 是数值最稳的求逆方法——对接近奇异的矩阵也能给出合理结果。

### 1.5 RMS 误差计算

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

**齐次归一化** `/= .at<double>(2,0)` 是投影变换的标准操作——把 `(x', y', w')` 变成 `(x'/w', y'/w', 1)`。

**只在 `inlier_matches` 上算**——不是全部 `good_matches`，避免误匹配拉高 RMS。

### 1.6 潜在问题

| 问题 | 说明 | 影响 |
| :--- | :--- | :--- |
| **`inliers_mask` 空检查不严密** | `findHomography` 返回空时 `inliers_mask` 可能是空的，`inliers_mask[i]` 越界 | 当前不触发（4 点 RANSAC 通常返回非空） |
| **`detectFeatures` 不处理灰度图** | `cv::cvtColor(image, gray, COLOR_BGR2GRAY)` 假设输入 BGR | 传入 `CV_8UC1` 会抛异常 |
| **`inlier_matches` 没返回** | `saveRegistrationResults` 保存的是 `good_matches`（含误匹配） | 调试时 `matches.txt` 不够干净 |
| **参数全硬编码** | ORB 5000 / RANSAC 3.0 / ratio 0.75 都不暴露 | 不同场景不能调 |

---

## 2. YUVImageReader.cpp —— YUV 读写

### 文件内部结构

```
构造函数         → 计算 y_size_ / uv_size_
readYUV420P      → YUV 文件 → BGR Mat
readYUV420PChannels  → YUV 文件 → 3 个独立 cv::Mat（⚠️ bug）
convertBGRtoYUV420P  → BGR → YUV 缓冲区
saveAsYUV420P    → BGR → YUV 文件
```

### 2.1 构造函数

```cpp
YUVImageReader::YUVImageReader(int width, int height)
    : width_(width), height_(height)
{
    y_size_ = width * height;
    uv_size_ = (width / 2) * (height / 2);
}
```

**实例是"尺寸绑定"的**——一个实例只能读/写 `width × height` 的 YUV。

### 2.2 `readYUV420P` —— 严格校验文件大小

```cpp
std::ifstream file(filepath, std::ios::binary | std::ios::ate);   // ate 模式：打开后定位到末尾
std::streamsize file_size = file.tellg();
file.seekg(0, std::ios::beg);

size_t expected_size = y_size_ + 2 * uv_size_;
if (file_size != expected_size) {
    throw std::runtime_error("文件大小与YUV420P格式不匹配");
}
```

**严格校验**——必须精确 `y_size_ + 2 * uv_size_` 字节。这对"每帧一个 YUV 文件"的存储格式是合理的，但**不支持多帧 YUV 文件**。

**转换流程**：

```
读 YUV 数据 → 创建 Y/U/V 平面视图
  → cv::resize(U) 到全尺寸（INTER_LINEAR）
  → cv::resize(V) 到全尺寸
  → cv::merge(Y, U, V) 成 3 通道
  → cv::cvtColor(COLOR_YUV2BGR)
```

**性能问题**：手动 `resize` 上采样 + `merge` + `cvtColor` 是三步。**更高效**：

```cpp
cv::Mat yuv420(height_ * 3 / 2, width_, CV_8UC1, yuv_data.data());
cv::cvtColor(yuv420, bgr_image, cv::COLOR_YUV2BGR_I420);
```

`COLOR_YUV2BGR_I420` 内部用 SIMD 一次完成 4:2:0 → BGR，**比手动 resize 快 3~5 倍**。当前每次配准只调 2 次 `readYUV420P`，影响不大，但对批量处理有优化空间。

### 2.3 🔴 `readYUV420PChannels` —— use-after-free bug

```cpp
std::vector<uint8_t> yuv_data(total_size);   // ← 局部 vector
file.read(reinterpret_cast<char*>(yuv_data.data()), total_size);

cv::Mat y_plane(height_, width_, CV_8UC1, yuv_data.data());   // ← 引用局部内存
cv::Mat u_plane_subsampled(..., yuv_data.data() + y_size_);
cv::Mat v_plane_subsampled(..., yuv_data.data() + y_size_ + uv_size_);

return {y_plane, u_plane_subsampled, v_plane_subsampled};   // ← 返回后 yuv_data 销毁
```

**`cv::Mat` 的外部数据构造（`cv::Mat(h, w, type, data)`）不接管 `data` 的所有权**。`yuv_data` 是局部 `std::vector`，函数返回后销毁，三个 `cv::Mat` 的 `data` 指针全部**悬空**。

**调用方拿到这三个 `cv::Mat` 后，任何 `.at()` / `.data` 访问都是未定义行为**——可能崩溃、可能读到垃圾、可能"看起来正常"（因为 vector 内存还没被覆盖）。

**修复**：

```cpp
cv::Mat y_plane(height_, width_, CV_8UC1, yuv_data.data());
y_plane = y_plane.clone();   // ← 深拷贝

cv::Mat u_plane_subsampled(height_/2, width_/2, CV_8UC1, yuv_data.data() + y_size_);
u_plane_subsampled = u_plane_subsampled.clone();
...
```

**当前不触发**：`LpReg.h` / `LpFovTransitionOpt.h` 里**没有调用 `readYUV420PChannels`**（只用 `readYUV420P`）。这是"预留接口 + bug"——一旦有人用就炸。

### 2.4 `convertBGRtoYUV420P` —— full-range 问题

```cpp
cv::cvtColor(bgr_image, yuv_image, cv::COLOR_BGR2YUV);   // ← full-range YUV
cv::split(yuv_image, channels);
cv::resize(channels[1], u_downsampled, ..., cv::INTER_AREA);
cv::resize(channels[2], v_downsampled, ..., cv::INTER_AREA);
std::memcpy(yuv_data.data(), channels[0].data, y_size_);
std::memcpy(yuv_data.data() + y_size_, u_downsampled.data, uv_size_);
std::memcpy(yuv_data.data() + y_size_ + uv_size_, v_downsampled.data, uv_size_);
```

**色彩范围问题**：

`cv::COLOR_BGR2YUV` 输出 **full-range**（Y 0~255，UV 0~255），而**视频标准（H.264/mp4）通常假设 limited-range**（Y 16~235，UV 16~240）。

| 阶段 | 色彩范围 |
| :--- | :--- |
| `saveAsYUV420P` | full-range |
| FFmpeg 编码 | 默认 limited-range |
| FFmpeg 解码 + `readYUV420P` | full-range |

**如果 YUV 经过 FFmpeg 编解码**，两端假设不一致，会有 10~15% 的亮度偏移。**这可能和"颜色偏黄"有关**——之前修的是 `sws_scale` 的 BT.709 矩阵，但**范围（range）问题没修**。

**更规范的做法**：

```cpp
// full-range BGR → limited-range I420（直接输出）
cv::cvtColor(bgr_image, yuv_image, cv::COLOR_BGR2YUV_I420);
```

或者保留 full-range，但 FFmpeg 编码时显式设 `color_range = AVCOL_RANGE_JPEG`。

**连续性假设**：

```cpp
std::memcpy(yuv_data.data(), channels[0].data, y_size_);
```

`cv::split` 出来的 `channels[0]` **继承父矩阵的 `step`**。如果 `bgr_image` 是 ROI 视图（非连续），`channels[0].data` 有 padding，`memcpy(y_size_)` 会读到下一行的 padding 数据，**输出错位**。

**当前安全**：`saveAsYUV420P` 里做了 `clone()` 或 `resize()`，保证传入的是连续 Mat。但**依赖调用方约定**，加一句 `CV_Assert(bgr_image.isContinuous())` 会更安全。

### 2.5 `saveAsYUV420P` —— 静默 resize

```cpp
if (image.cols != width_ || image.rows != height_) {
    cv::resize(image, resized_image, cv::Size(width_, height_));   // ← INTER_LINEAR
} else {
    resized_image = image.clone();
}
```

**尺寸不匹配时 resize**——**没告诉调用方**（静默缩放）。

**`INTER_LINEAR` 对大比例缩放质量差**。如果输入 4K 输出 720p，会有摩尔纹。应该用 `INTER_AREA`（下采样）或 `INTER_CUBIC`（上采样）。**当前输入输出尺寸通常一致**，不触发。

---

## 3. 历史修复汇总

### 已修复

| 位置 | 问题 | 修复 |
| :--- | :--- | :--- |
| `FFmpegDefine.h` | 全局 `std::string` 静态初始化顺序 | 改成"函数内 static" |
| `DecAndEnc.h` | 多个函数用裸指针 + 手动释放 | `DecodeAndEncodeAndReplaceMultiYuvs` 用完整 RAII |
| `DecAndEnc.h` | `DecodeEncode` 400kbps 硬编码 | `ReplaceMultiYuvs` 用 libx264 + CRF 20 |
| `DecAndEnc.h` | `Close` 漏 `av_packet_unref`（在 FfmpegVideoWriter 里） | 补上 |
| `ImageRegistrator.cpp` | 逆矩阵方向错误（常见坑） | 正确用 `warp_to_ref` |

### 未修复（已知问题）

| 位置 | 问题 | 影响 |
| :--- | :--- | :--- |
| `YUVImageReader::readYUV420PChannels` | use-after-free | 一调用就炸（当前未调用） |
| `DecodeAndEncodeAndInsertMultiYuvs` | `AVFramePtr` 接管后手动 `av_frame_free` → double free | 崩溃 |
| `DecodeEncode` / `InsertSingleYuv` / `InsertMultiYuvs` | 400kbps 硬编码 | 画质差 |
| `YUVImageReader` 色彩范围 | full-range in/out，FFmpeg 编码 default limited-range | 亮度偏移 ~10% |
| `CROP_COVER_DIR` | 双斜杠路径 | Windows 无害，跨平台可能报错 |
| `DecodeCropEncodeInputVid` | `1562` 硬编码与 `YUV_H = 1584` 不一致 | 裁剪结果与下游不匹配 |
| `ReplaceAlhpaMergeToFovSmoothAndAlpaMergedLp` | 与 `ReplaceAlhpaMergeToFovSmoothLp` 逐字相同 | 死代码 |
| `YUVProcessor::processPlane` | `y1` 上界误用 `crop_x + crop_width` | 当前不触发，潜在定时炸弹 |
| `DecodeAndEncodeAndReplaceMultiYuvs` | 不校验 `replaceYuvFiles.size()` 与 `[start, end]` 长度 | 参数错时静默出错 |
| `regCroppedConverYuvsToVidFrame` | `alt_jpg` / `src_yuv_secondary` 是死代码 | — |
| `GenMp4.h` | 300 行死代码，无 RAII | 无用 include 增加编译时间 |
| `YUVProcessor::ConvertCoverFrameToMultiFovYuvs` | 1920×1080 硬编码 | 误调用会全错位 |

---

## 相关文档

- **模块总览**：[`README.md`](./README.md) —— 文件结构、数据流、性能分析、调试方法
- **接口说明**：[`../../inc/LivePhotoSmoothPlay/README.md`](../../inc/LivePhotoSmoothPlay/README.md) —— 每个类的公开 API、使用示例、注意事项
- **测试说明**：[`../../tests/LivePhotoSmoothPlay/README.md`](../../tests/LivePhotoSmoothPlay/README.md) —— 测试入口、依赖数据、运行方式