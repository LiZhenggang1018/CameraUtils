# inc/LivePhotoSmoothPlay —— LivePhoto 平滑播放特性（接口层）

## 概述

本目录是 **LivePhoto 平滑播放**特性的头文件层——把 LivePhoto（用 mp4 模拟）原本"突兀切到封面帧"的播放，改成 **FOV 渐变推近/拉远**的平滑过渡。

**核心思路**：在"最后视频帧"与"封面帧"之间，用 **中心裁剪 + 放大回原尺寸** 生成 20 帧 FOV 渐变帧，替换掉原有的 alpha 混合过渡帧。

**依赖**：FFmpeg（libx264 编解码）、OpenCV（图像配准、warp、crop、alpha 混合）。

---

## 业务流程

```
Lp.mp4（原始 LP，含 alpha 混合过渡帧）
    │
    ├─ 1. DecodeLpMp4ToYuvs()
    │       ↓ Lp.mp4 → LpJpg/{1.yuv, 2.yuv, ...}
    │
    ├─ 1.5 【手动】改 FFmpegDefine.h 的 LAST_VID_FRAME_IDX / COVER_FRAME_IDX → 重编译
    │
    ├─ 2. RegCoverToVid()
    │       ↓ 封面帧 ↔ 最后视频帧的 ORB 配准
    │       ↓ 输出：REG_RESULT_DIR/（10 个调试文件）
    │              vidToCoverScaleRatio.txt（x, y 缩放比例）
    │
    ├─ 3. GenDiffFovYuvsFromCoverFrame()
    │       ↓ 封面帧 → crop1.yuv ... crop20.yuv（20 个 FOV 渐变帧）
    │
    ├─ 4. ReplaceAlhpaMergeToFovSmoothLp()
    │       ↓ [276, 295] 区间替换为 crop20..crop1.yuv
    │       ↓ 输出：LpOpt.mp4
    │
    └─ 5. DecodeOptLpMp4ToYuvs()
            ↓ LpOpt.mp4 → LpOptJpg/{1.yuv, 2.yuv, ...}（验证用）
```

---

## 文件结构总览

| 文件 | 类型 | 职责 |
| :--- | :--- | :--- |
| `LpFovTransitionOpt.h` | 编排 | **主入口**，串起 5 步业务流程 |
| `DecDemo.h` | 编排 | 4 个业务操作（解码/替换） |
| `LpReg.h` | 编排 | 配准编排（单帧 + 批量） |
| `CenterCropGenYuvs.h` | 算法 | FOV 渐变过渡帧生成 |
| `AlphaMergeYuvs.h` | 工具 | YUV 内存结构 + alpha 混合 |
| `ImageRegistrator.h` | 类声明 | 图像配准（ORB + HOMOGRAPHY） |
| `YUVImageReader.h` | 类声明 | YUV420P ↔ BGR 读写 |
| `FFmpegDefine.h` | 常量 | 路径 + 参数 + RAII 删除器 |
| `FFMpegUtils.h` | 工具 | 统计 I/P/B 帧数（已修 flush） |
| `DumpUtils.h` | 工具 | 模板 Dump 调试 |
| `YUVProcessor.h` | 工具 | YUV 裁剪（**当前无调用**） |


---

## 各文件接口

### `FFmpegDefine.h` —— 路径与常量

#### 路径宏

| 宏 | 相对 `LP_SMOOTH_PLAY_DATA_PATH` | 用途 |
| :--- | :--- | :--- |
| `S_LYF_FILE` | `smallLyf.mp4` | 小 FOV 视频 |
| `B_LYF_FILE` | `bigLyf.mp4` | 大 FOV 视频（封面帧来源） |
| `LP_INIT_FILE` | `Lp.mp4` | **原始 LP（主流程输入）** |
| `LP_OPT_FILE` | `LpOpt.mp4` | **优化 LP（主流程输出）** |
| `LP_CROP_FILE` | `LpCrop.mp4` | 录屏裁剪输出 |
| `LP_DECODE_YUV_DIR` | `LpJpg` | LP 解码 YUV 目录 |
| `CROP_COVER_DIR` | `CropCover` | FOV 渐变帧目录 |
| `LAST_VID_FRAME_YUV` | `LpJpg/275.yuv` | 最后视频帧 |
| `COVER_FRAME_YUV` | `LpJpg/296.yuv` | 封面帧 |
| `REG_RESULT_DIR` | `coverToVidRegResult` | 配准结果目录 |
| `vidToCoverScaleRatioFile` | `vidToCoverScaleRatio.txt` | 缩放比例文件 |
| `CROP_REG_RESULT_DIR` | `cropReg/cropReg` | 过渡帧配准结果 |
| `YUV_WH_FILE` | `yuvWidthHeight.txt` | YUV 宽高记录 |

#### 编译期常量

| 常量 | 值 | 语义 |
| :--- | :---: | :--- |
| `LAST_VID_FRAME_IDX` | 275 | 视频最后一帧索引（**手动指定**） |
| `TransFrameNum` | 20 | 过渡帧数量 |
| `YUV_W` / `YUV_H` | 720 / 1584 | YUV 分辨率 |
| `COVER_FRAME_IDX` | 296 | 封面帧索引（`LAST_VID_FRAME_IDX + 21`） |
| `TO_REPLACE_YUV_START_IDX` | 276 | 替换区间起点 |
| `TO_REPLACE_YUV_END_IDX` | 295 | 替换区间终点 |

#### RAII 删除器 + 类型别名

```cpp
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr  = std::unique_ptr<AVCodecContext,  AVCodecContextDeleter>;
using AVPacketPtr        = std::unique_ptr<AVPacket,        AVPacketDeleter>;
using AVFramePtr         = std::unique_ptr<AVFrame,         AVFrameDeleter>;
using SwsCtxPtr          = std::unique_ptr<SwsContext,      SwsContextDeleter>;
using FilePtr            = std::unique_ptr<FILE,            FileDeleter>;
```

全部 `noexcept` + 空指针保护，是整个模块的 **RAII 基础设施**。

### `FFMpegUtils.h` —— 帧统计

```cpp
std::tuple<int, int, int> CountFrames(const std::string& input_file);
```

返回 `(iCount, pCount, bCount)`。


### `DecAndEnc.h` —— FFmpeg 编解码

| 函数 | 用途 |
| :--- | :--- |
| `GetAVStreamTempInfo(filename)` | 读视频流元信息（time_base / 宽高 / pix_fmt / framerate） |
| `DecodeAndEncodeAndReplaceMultiYuvs(...)` | **主流程用**：解码 + 替换指定区间 + libx264 CRF20 编码 |
| `DecodeMp4ToYuvFrames(filename, outDir)` | 解码 mp4 → 逐帧 `{N}.yuv` + `{N}.jpg` |
| `DecodeCropEncode(input, output, xRatio, yRatio)` | 转码 + 中心裁剪 |

**`DecodeAndEncodeAndReplaceMultiYuvs` 签名**：


### `YUVImageReader.h` —— YUV 读写

```cpp
class YUVImageReader {
public:
    YUVImageReader(int width, int height);

    cv::Mat readYUV420P(const std::string& filepath);              // YUV → BGR
    std::vector<cv::Mat> readYUV420PChannels(const std::string&);  // 3 个独立通道（已修 use-after-free）
    void saveAsYUV420P(const cv::Mat& image, const std::string&);  // BGR → YUV
};
```
` |

### `AlphaMergeYuvs.h` —— YUV 工具

#### `YuvInfo` 结构体

```cpp
struct YuvInfo {
    unsigned char* pData;    // YUV 内存首地址
    int width;
    int height;
    int stride;              // 必须 >= width
};
```

| 函数 | 用途 |
| :--- | :--- |
| `DumpYuv` / `DumpYuv2Jpg` | YUV → 文件 / JPG |
| `Yuv2BgrMat` | YUV → BGR Mat |
| `AlphaMergeYuvs` | 纯 C++ alpha 混合（I420） |
| `AlphaMergeYuvsOpenCV` | OpenCV alpha 混合（NV12） |
| `LoadYuvFromFile` / `FreeYuvInfo` | 文件读 / 内存释放 |

**⚠️ 注意**：`AlphaMergeYuvs`（I420）与 `AlphaMergeYuvsOpenCV`（NV12）**UV 布局假设不同**，当前业务只用 I420 版本。

### `DumpUtils.h` —— 模板 Dump

```cpp
template<typename... Args>
void Dump(const std::string& fileName, Args&&... args);
```

在 `DecAndEnc.h` 里用 `Dump(YUV_WH_FILE, width, height)` 记录分辨率。

### `ImageRegistrator.h` —— 图像配准

```cpp
class ImageRegistrator {
public:
    RegistrationResult registerImages(
        const cv::Mat& reference_img,
        const cv::Mat& target_img,
        FeatureDetectorType detector_type = FeatureDetectorType::ORB,
        TransformType transform_type = TransformType::HOMOGRAPHY);

    cv::Mat drawMatches(...);
    void saveRegistrationResults(...);
};
```

#### 支持的检测器 / 变换

| `FeatureDetectorType` | 特点 |
| :--- | :--- |
| `ORB`（默认） | 免费，快，**5000 个特征点** |
| `BRISK` / `KAZE` / `AKAZE` | 免费，主库 |

| `TransformType` | 自由度 |
| :--- | :---: |
| `HOMOGRAPHY`（默认） | 8 |
| `AFFINE` | 6 |
| `EUCLIDEAN` | 4 |
| `TRANSLATION` | 2 |

#### `RegistrationResult` 关键字段

```cpp
cv::Mat registered_image;        // 变换后的目标图（target 对齐到 reference）
cv::Mat transform_matrix;        // reference → target 的矩阵
int num_inliers;                 // RANSAC 内点数（质量指标）
double rms_error;                // 重投影均方误差（质量指标）
```

**重要**：`transform_matrix` 是 `reference → target`，**`registerImages` 内部已求逆**得到 `warp_to_ref` 再 warp——外部直接用 `registered_image` 即可。

### `LpReg.h` —— 配准编排

| 函数 | 用途 |
| :--- | :--- |
| `RegCoverToVid()` | 封面帧 ↔ 最后视频帧配准，产出 `vidToCoverScaleRatio.txt` |
| `RegCoverFrameToLastVidFrame(...)` | 单次配准 + 10 个调试产物 |
| `RegCroppedConverYuvsToVidFrame()` | 20 个 crop 帧分别配准（**可选，调试用**） |
| `DecodeCropEncodeInputVid()` | 录屏裁边（**⚠️ `1562` 硬编码，仅适用于 X300U 录屏**） |
| `GetXYScaleFroMatrixVidToCover()` | 从 txt 读 `(x, y)` 缩放比例 |
| `DumpXYScaleFroMatrixVidToCover()` | 从 H 矩阵写 `(x, y)` 到 txt |

**已修复**：`estimateTransform` 里 `inliers_mask` 空检查；`detectFeatures` 兼容 BGR/BGRA/GRAY 三种输入。

### `CenterCropGenYuvs.h` —— FOV 渐变生成

```cpp
void CenterCropGenYuvs(
    const std::string& inputYuv,
    int w, int h,
    int num,
    double cropRatio,        // 每帧单边裁剪比例增量
    const std::string& outDir);

void GenDiffFovYuvsFromCoverFrame();   // 无参入口
```

**`cropRatio` 推导**（在 `GenDiffFovYuvsFromCoverFrame`）：

```cpp
ratio = (1 - vidToCoverScaleRatio) / 2 / TransFrameNum;
```

**FOV 渐变语义**：

```
crop1.yuv   ← 裁最少 → FOV 最大 → 最接近封面帧
crop20.yuv  ← 裁最多 → FOV 最小 → 最接近视频帧
```

### `YUVProcessor.h` —— 当前无调用

| 内容 | 状态 |
| :--- | :---: |
| `YUVProcessor` 类 | 无调用，保留 |
| `ConvertCoverFrameToMultiFovYuvs` | 死代码（1920×1080 硬编码） |

### `DecDemo.h` —— 业务编排（清理后 4 个函数）

| 函数 | 输入 | 输出 |
| :--- | :--- | :--- |
| `DecodeCoverMp4ToYuvs` | `B_LYF_FILE` | CWD 下 `N.yuv`（辅助，主流程不调） |
| `DecodeLpMp4ToYuvs` | `LP_INIT_FILE` | `LP_DECODE_YUV_DIR` |
| `DecodeOptLpMp4ToYuvs` | `LP_OPT_FILE` | `LpOptJpg/` |
| `ReplaceAlhpaMergeToFovSmoothLp` | `LP_INIT_FILE` | `LP_OPT_FILE` |

### `LpFovTransitionOpt.h` —— 主入口

```cpp
inline void GenFromLpToOptLp()          // 完整业务流程
inline void LpFovTransitionOptTest()    // 测试入口
```

---

## 调用时序

```
LpFovTransitionOptTest()
  └─ GenFromLpToOptLp()
       ├─ DecodeLpMp4ToYuvs()
       │     └─ DecodeMp4ToYuvFrames(LP_INIT_FILE, LP_DECODE_YUV_DIR)
       │
       ├─ [手动] 改 LAST_VID_FRAME_IDX / COVER_FRAME_IDX → 重编译
       │
       ├─ RegCoverToVid()
       │     └─ RegCoverFrameToLastVidFrame(YUV_W, YUV_H, LAST_VID_FRAME_YUV, COVER_FRAME_YUV, REG_RESULT_DIR)
       │           ├─ YUVImageReader::readYUV420P × 2
       │           ├─ ImageRegistrator::registerImages(ORB, HOMOGRAPHY)
       │           ├─ DumpXYScaleFroMatrixVidToCover() → vidToCoverScaleRatio.txt
       │           └─ saveRegistrationResults + 8 个调试 jpg
       │
       ├─ GenDiffFovYuvsFromCoverFrame()
       │     ├─ GetXYScaleFroMatrixVidToCover()
       │     ├─ ratio = (1 - avg(x,y)) / 2 / TransFrameNum
       │     └─ CenterCropGenYuvs(COVER_FRAME_YUV, YUV_W, YUV_H, 20, ratio, CROP_COVER_DIR)
       │
       ├─ ReplaceAlhpaMergeToFovSmoothLp()
       │     ├─ 构造 replaceYuvFiles = [crop20, crop19, ..., crop1]   ← 逆序
       │     └─ DecodeAndEncodeAndReplaceMultiYuvs(
       │             LP_INIT_FILE, LP_OPT_FILE,
       │             TO_REPLACE_YUV_START_IDX, TO_REPLACE_YUV_END_IDX,
       │             replaceYuvFiles)
       │
       └─ DecodeOptLpMp4ToYuvs()
             └─ DecodeMp4ToYuvFrames(LP_OPT_FILE, LpOptJpg)
```

---

## 注意事项

### 🔴 必须了解的前置条件

1. **`LAST_VID_FRAME_IDX` 和 `COVER_FRAME_IDX` 是编译期常量**
   - 每换一个 LP 视频，要手动改 `FFmpegDefine.h` 然后**重编译整个项目**
   - 索引需人眼识别——没有自动化

2. **YUV 分辨率必须匹配 `YUV_W` / `YUV_H`（720×1584）**
   - `CenterCropGenYuvs`、`AlphaMergeYuvs` 全部按硬编码尺寸处理

3. **输入视频的 time_base 直接影响 pts 计算**
   - `ReplaceMultiYuvs` 里 `last_decoded_pts += 1`
   - 如果 time_base 是 1/90000（常见 mp4 实际 time_base），`+1` ≈ `1/90000` 秒，替换帧会挤在同一时刻

### 🟡 `cropRatio` 语义三处不一致

| 函数 | 参数语义 | 单位 |
| :--- | :--- | :--- |
| `CenterCropGenYuvs` | **单边**裁剪比例 | `[0, 0.5]` |
| `DecodeCropEncode` | **总**裁剪比例 | `[0, 1)` |
| `YUVProcessor::cropAndResize` | **每边**裁剪百分比 | `(0, 100)` |

### 🟡 隐式契约

| 契约 | 生产方 | 消费方 |
| :--- | :--- | :--- |
| `vidToCoverScaleRatio.txt` 格式：`x y\n` | `DumpXYScaleFroMatrixVidToCover` | `GetXYScaleFroMatrixVidToCover` |
| `cropN.yuv` 命名 | `CenterCropGenYuvs` | `DecDemo::ReplaceAlhpaMergeToFovSmoothLp` |
| `crop1` FOV 最窄、`crop20` 最宽 | `CenterCropGenYuvs` | `Replace` 里逆序构造 |
| `g_hMatrixVidToCover` 全局状态 | `RegCoverFrameToLastVidFrame` | `DumpXYScaleFroMatrixVidToCover` |
| `TransFrameNum = 20` 且 `[276, 295]` 长度 = 20 | `FFmpegDefine.h` | `ReplaceMultiYuvs`（**已加校验**） |

### ⚠️ 保留的已知项

| 位置 | 说明 |
| :--- | :--- |
| `DecodeCropEncodeInputVid` 的 `1562` / `1260` / `300` | 对应 X300U 录屏工具，**换录屏必须手动改** |
| `YUVProcessor::ConvertCoverFrameToMultiFovYuvs` | 死代码（1920×1080 硬编码），无调用 |

### 🟢 已修复汇总

| 位置 | 修复 |
| :--- | :--- |
| `YUVImageReader::readYUV420PChannels` | 深拷贝，消除 use-after-free |
| `YUVImageReader` 色彩范围 | `COLOR_BGR2YUV_I420` / `COLOR_YUV2BGR_I420`，与 FFmpeg limited-range 一致 |
| `CROP_COVER_DIR` 双斜杠 | 去前导 `/` |
| `processPlane` 的 `y1` 边界 | `crop_y + crop_height - 1` |
| `ImageRegistrator::estimateTransform` | `inliers_mask` 空检查 |
| `ImageRegistrator::detectFeatures` | 兼容 BGR/BGRA/GRAY |
| `ReplaceMultiYuvs` | `replaceYuvFiles.size()` 校验 |
| `CountFrames` | 加 flush decoder，最后 1~3 帧不再漏统计 |
| 死代码清理 | 删 `GenMp4.h`、`DecodeEncode`、`InsertSingleYuv`、`InsertMultiYuvs`、`GenOriginalLivePhoto`、`GenOptLivePhoto`、`ReplaceAlhpaMerge*` 重复函数、`GenFrom2Mp4ToOriAndOptLp` |

---

## 扩展指引

| 想做的 | 需要改 |
| :--- | :--- |
| 支持任意视频 | 把 `LAST_VID_FRAME_IDX` 从 `constexpr` 改为运行时配置 |
| 自动检测最后视频帧 | 用帧间差异 / PTS 分析替代人工识别 |
| 支持任意分辨率 | `YUV_W` / `YUV_H` 从 `yuvWidthHeight.txt` 运行时读 |
| 提高配准精度 | `ImageRegistrator` 加 SIFT（主库可用）；ORB 5000 可调 |
| 批量处理 | `g_hMatrixVidToCover` 改为函数返回；`current_path()` 改为显式路径 |
| 提高编码质量 | `ReplaceMultiYuvs` 的 `bit_rate = 4000000` 和 CRF 20 可调 |

---

## 相关文档

- **实现总览**：[`../../src/LivePhotoSmoothPlay/README.md`](../../src/LivePhotoSmoothPlay/README.md)
- **实现细节**：[`../../src/LivePhotoSmoothPlay/IMPLEMENTATION_DETAILS.md`](../../src/LivePhotoSmoothPlay/IMPLEMENTATION_DETAILS.md)
- **测试说明**：[`../../tests/LivePhotoSmoothPlay/README.md`](../../tests/LivePhotoSmoothPlay/README.md)
- **配置说明**：[`../../configs/README.md`](../../configs/README.md)