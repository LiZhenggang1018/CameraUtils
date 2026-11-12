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

| 文件 | 类型 | 职责 | 状态 |
| :--- | :--- | :--- | :---: |
| `LpFovTransitionOpt.h` | 编排 | **主入口**，串起 6 步业务流程 | ✅ 生产 |
| `DecDemo.h` | 编排 | 6 个业务操作（解码/生成/替换） | ✅ 生产 |
| `LpReg.h` | 编排 | 配准编排（单帧 + 批量） | ✅ 生产 |
| `CenterCropGenYuvs.h` | 算法 | FOV 渐变过渡帧生成 | ✅ 生产 |
| `AlphaMergeYuvs.h` | 工具 | YUV 内存结构 + alpha 混合 | ✅ 部分 |
| `ImageRegistrator.h` | 类声明 | 图像配准（ORB + HOMOGRAPHY） | ✅ 生产 |
| `YUVImageReader.h` | 类声明 | YUV420P ↔ BGR 读写 | ✅ 生产 |
| `FFmpegDefine.h` | 常量 | 路径 + 参数 + RAII 删除器 | ✅ 生产 |
| `FFMpegUtils.h` | 工具 | 统计 I/P/B 帧数 | ✅ 辅助 |
| `DumpUtils.h` | 工具 | 模板 Dump 调试 | ⚠️ 含大量示例 |
| `YUVProcessor.h` | 工具 | YUV 裁剪（旧版） | ⚠️ 废弃 |
| `GenMp4.h` | 独立 demo | 生成测试 mp4 | ❌ 废弃 |
| `README.md` | 本文档 | — | — |

---

## 各文件接口说明

### `FFmpegDefine.h` —— 路径与常量

#### 路径宏（全部 `函数内 static` + `#define`，已修复静态初始化顺序问题）

| 宏 | 相对 `LP_SMOOTH_PLAY_DATA_PATH` 的路径 | 用途 |
| :--- | :--- | :--- |
| `S_LYF_FILE` | `smallLyf.mp4` | 小 FOV 视频（输入） |
| `B_LYF_FILE` | `bigLyf.mp4` | 大 FOV 视频（封面帧来源） |
| `ORI_FILE` | `ori.mp4` | 原始 LP 输出（旧方案） |
| `OPT_FILE` | `opt.mp4` | 优化 LP 输出（旧方案） |
| `LP_INIT_FILE` | `Lp.mp4` | **原始 LP（新方案输入）** |
| `LP_OPT_FILE` | `LpOpt.mp4` | **优化 LP（新方案输出）** |
| `LP_DECODE_YUV_DIR` | `LpJpg` | LP 解码的 YUV 输出目录 |
| `CROP_COVER_DIR` | `/CropCover` | **⚠️ 注意前导 `/`（双斜杠）** |
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
| `COVER_FRAME_IDX` | 296 | 封面帧索引（275 + 21） |
| `TO_REPLACE_YUV_START_IDX` | 276 | 替换区间起点 |
| `TO_REPLACE_YUV_END_IDX` | 295 | 替换区间终点（20 帧，对应 `TransFrameNum`） |

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

返回 `(iCount, pCount, bCount)`，用于对比"输入 vs 输出"的 I/P/B 帧分布。

### `DecAndEnc.h` —— FFmpeg 编解码

| 函数 | 用途 | 状态 |
| :--- | :--- | :---: |
| `GetAVStreamTempInfo` | 读视频流元信息 | ✅ |
| `DecodeEncode` | 纯转码（无插入/替换） | ❌ 废弃 |
| `DecodeMp4ToYuvFrames` | 解码 mp4 → 逐帧 YUV 文件 | ✅ 工具 |
| `DecodeCropEncode` | 转码 + 中心裁剪 | ✅ 新功能 |

**生产用的 `DecodeAndEncodeAndReplaceMultiYuvs`**：

```cpp
void DecodeAndEncodeAndReplaceMultiYuvs(
    const char* input_filename,
    const char* output_filename,
    int toReplaceYuvStartIdx,         // 起始帧索引（1-based）
    int toReplaceYuvEndIdx,           // 结束帧索引（含）
    const std::vector<std::string>& replaceYuvFiles);   // 替换用的 YUV 路径序列
```

- 用 **libx264 + CRF 20** 编码
- 完整 RAII
- **替换序列数量必须等于 `end - start + 1`**，否则静默出错（无校验）

### `YUVImageReader.h` —— YUV 读写

```cpp
class YUVImageReader {
public:
    YUVImageReader(int width, int height);

    cv::Mat readYUV420P(const std::string& filepath);              // YUV → BGR
    std::vector<cv::Mat> readYUV420PChannels(const std::string&);  // ⚠️ use-after-free bug
    void saveAsYUV420P(const cv::Mat& image, const std::string&);  // BGR → YUV
};
```

**要求**：`width`/`height` 与 YUV 文件实际尺寸一致，否则抛异常。

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

#### 提供的函数

| 函数 | 用途 | 状态 |
| :--- | :--- | :---: |
| `DumpYuv` | YUV 数据 → 文件 | ✅ |
| `DumpYuv2Jpg` | YUV 数据 → JPG | ✅ |
| `Yuv2BgrMat` | YUV 数据 → BGR Mat | ✅ |
| `DumpYuv2JpgOptimized` | 优化版 Dump（`vconcat` 尺寸 bug，**死代码**） | ❌ |
| `AlphaMergeYuvs` | 纯 C++ alpha 混合（**I420**） | ✅ |
| `AlphaMergeYuvsOpenCV` | OpenCV alpha 混合（**NV12**，与上者 UV 布局不一致） | ⚠️ |
| `LoadYuvFromFile` / `FreeYuvInfo` | 文件读 / 内存释放 | ✅ |
| `TestAlphaMergeYuvs` | 测试 | ✅ |

**⚠️ 关键坑**：`AlphaMergeYuvs` 和 `AlphaMergeYuvsOpenCV` 对 UV 布局的假设**不同**——前者按 I420（U/V 平面分离），后者按 NV12（UV 交织）。当前业务只用 `AlphaMergeYuvs`（I420，正确）。

### `DumpUtils.h` —— 模板 Dump

```cpp
template<typename... Args>
void Dump(const std::string& fileName, Args&&... args);
```

写参数到文件 + 回读打印。**当前在 `DecAndEnc.h` 里用 `Dump(YUV_WH_FILE, width, height)` 记录分辨率**。

**注意**：文件里 60%+ 是"教学示例"和测试函数，每个 include 它的翻译单元都会编译这些代码。

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
| `BRISK` | 免费，中速 |
| `KAZE` / `AKAZE` | 免费，主库 |

| `TransformType` | 自由度 |
| :--- | :---: |
| `HOMOGRAPHY`（默认） | 8 |
| `AFFINE` | 6 |
| `EUCLIDEAN` | 4 |
| `TRANSLATION` | 2 |

#### `RegistrationResult` 关键字段

```cpp
cv::Mat registered_image;        // 变换后的目标图
cv::Mat transform_matrix;        // reference → target 的矩阵
int num_inliers;                 // RANSAC 内点数（质量指标）
double rms_error;                // 重投影均方误差（质量指标）
std::vector<cv::DMatch> good_matches;    // Lowe's ratio 过滤后
```

**重要**：`transform_matrix` 是 `reference → target`，**要 warp target 到 reference 需要求逆**——`registerImages` 内部已处理。

### `LpReg.h` —— 配准编排

| 函数 | 用途 |
| :--- | :--- |
| `RegCoverToVid()` | 封面帧 ↔ 最后视频帧配准，产出 `vidToCoverScaleRatio.txt` |
| `RegCoverFrameToLastVidFrame(...)` | 单次配准 + 10 个调试产物 |
| `RegCroppedConverYuvsToVidFrame()` | 20 个 crop 帧分别配准（**可选，调试用**） |
| `DecodeCropEncodeInputVid()` | 录屏裁边（**⚠️ 硬编码 1562**） |
| `GetXYScaleFroMatrixVidToCover()` | 从 txt 读 (x, y) 缩放比例 |
| `DumpXYScaleFroMatrixVidToCover()` | 从 H 矩阵写 (x, y) 到 txt |

**`vidToCoverScaleRatio.txt`** 是 `LpReg.h` 和 `CenterCropGenYuvs.h` 的**隐式契约**——一个写，一个读。

### `CenterCropGenYuvs.h` —— FOV 渐变生成

```cpp
void CenterCropGenYuvs(
    const std::string& inputYuv,   // 输入 YUV（封面帧）
    int w, int h,                  // 输入尺寸
    int num,                       // 生成数量（=TransFrameNum）
    double cropRatio,              // 每帧的单边裁剪比例增量
    const std::string& outDir);    // 输出目录

void GenDiffFovYuvsFromCoverFrame();   // 无参入口，从配准结果推导裁剪比例
```

**FOV 渐变机制**：

```
crop1.yuv   ← 中心裁 2*1*ratio，resize 回全幅（最接近封面，zoom-in 最大）
crop2.yuv   ← 中心裁 2*2*ratio，resize 回全幅
...
crop20.yuv  ← 中心裁 2*20*ratio，resize 回全幅（FOV 最接近视频帧）
```

**`ratio` 的推导**（在 `GenDiffFovYuvsFromCoverFrame`）：

```cpp
ratio = (1 - vidToCoverScaleRatio) / 2 / TransFrameNum;
```

**语义**：总裁剪步长 = `1 - vidToCoverScaleRatio`（约 6%），每帧步长 = `总裁剪 / TransFrameNum`。

### `YUVProcessor.h` / `GenMp4.h` —— 废弃

| 文件 | 内容 | 状态 |
| :--- | :--- | :---: |
| `YUVProcessor.h` | `YUVProcessor` 类（YUV 裁剪）+ `ConvertCoverFrameToMultiFovYuvs`（**旧版 FOV 生成，1920×1080 硬编码**） | ⚠️ 类保留，旧函数废弃 |
| `GenMp4.h` | `GenSimpleMp4()` 生成渐变条纹测试视频 | ❌ 完全废弃 |

### `DecDemo.h` —— 业务编排

| 函数 | 输入 | 输出 |
| :--- | :--- | :--- |
| `DecodeCoverMp4ToYuvs` | `B_LYF_FILE` | CWD 下 `N.yuv` |
| `DecodeLpMp4ToYuvs` | `LP_INIT_FILE` | `LP_DECODE_YUV_DIR` |
| `DecodeOptLpMp4ToYuvs` | `LP_OPT_FILE` | `LpOptJpg/` |
| `GenOriginalLivePhoto` | `S_LYF_FILE` | `ORI_FILE` |
| `GenOptLivePhoto` | `S_LYF_FILE` | `OPT_FILE` |
| `ReplaceAlhpaMergeToFovSmoothLp` | `LP_INIT_FILE` | `LP_OPT_FILE` |
| `ReplaceAlhpaMergeToFovSmoothAndAlpaMergedLp` | **⚠️ 与上者完全重复** | `LP_OPT_FILE` |

### `LpFovTransitionOpt.h` —— 主入口

```cpp
inline void GenFromLpToOptLp()   // 完整业务流程（6 步）
inline void LpFovTransitionOptTest()   // 测试入口，调 GenFromLpToOptLp
```

---

## 调用时序

```
LpFovTransitionOptTest()
  └─ GenFromLpToOptLp()
       ├─ DecodeLpMp4ToYuvs()
       │     └─ DecodeMp4ToYuvFrames(LP_INIT_FILE, LP_DECODE_YUV_DIR)
       │           └─ FFmpeg 解码 + 逐帧存 {N}.yuv + {N}.jpg
       │
       ├─ [手动] 改 LAST_VID_FRAME_IDX / COVER_FRAME_IDX → 重编译
       │
       ├─ RegCoverToVid()
       │     └─ RegCoverFrameToLastVidFrame(YUV_W, YUV_H, LAST_VID_FRAME_YUV, COVER_FRAME_YUV, REG_RESULT_DIR)
       │           ├─ YUVImageReader::readYUV420P × 2
       │           ├─ ImageRegistrator::registerImages(ORB, HOMOGRAPHY)
       │           │     └─ 内部：detectFeatures → matchFeatures → estimateTransform → 求逆 → applyTransform
       │           ├─ DumpXYScaleFroMatrixVidToCover() → vidToCoverScaleRatio.txt
       │           └─ saveRegistrationResults + 8 个调试 jpg
       │
       ├─ GenDiffFovYuvsFromCoverFrame()
       │     ├─ GetXYScaleFroMatrixVidToCover()   ← 读 txt
       │     ├─ ratio = (1 - avg(x,y)) / 2 / TransFrameNum
       │     └─ CenterCropGenYuvs(COVER_FRAME_YUV, YUV_W, YUV_H, 20, ratio, CROP_COVER_DIR)
       │           └─ crop1.yuv ... crop20.yuv + 对应 jpg
       │
       ├─ ReplaceAlhpaMergeToFovSmoothLp()
       │     ├─ 构造 replaceYuvFiles = [crop20, crop19, ..., crop1]   ← 逆序
       │     └─ DecodeAndEncodeAndReplaceMultiYuvs(
       │             LP_INIT_FILE, LP_OPT_FILE,
       │             TO_REPLACE_YUV_START_IDX, TO_REPLACE_YUV_END_IDX,
       │             replaceYuvFiles)
       │           └─ 解码 Lp.mp4 → [276, 295] 替换 → libx264 CRF20 编码 → LpOpt.mp4
       │
       └─ DecodeOptLpMp4ToYuvs()
             └─ DecodeMp4ToYuvFrames(LP_OPT_FILE, LP_OPT_FILE/Jpg)
```

---

## 注意事项

### 🔴 必须了解的前置条件

1. **`LAST_VID_FRAME_IDX` 和 `COVER_FRAME_IDX` 是编译期常量**
   - 每换一个 LP 视频，要手动改 `FFmpegDefine.h` 然后**重编译整个项目**
   - 索引需人眼识别——没有自动化

2. **YUV 分辨率必须匹配 `YUV_W` / `YUV_H`（720×1584）**
   - `CenterCropGenYuvs`、`AlphaMergeYuvs` 全部按硬编码尺寸处理
   - 如果输入不是 720×1584，会全部错位

3. **输入视频的 time_base 直接影响 pts 计算**
   - `DecodeAndEncodeAndReplaceMultiYuvs` 里 `last_decoded_pts += 1`
   - 如果 time_base 是 1/90000，`+1` ≈ `1/90000` 秒，替换帧会挤在同一时刻
   - 如果是 1/90（90fps 录屏），`+1` = 一帧，正确

### 🟡 隐式契约

| 契约 | 生产方 | 消费方 |
| :--- | :--- | :--- |
| `vidToCoverScaleRatio.txt` 格式：`x y\n` | `DumpXYScaleFroMatrixVidToCover` | `GetXYScaleFroMatrixVidToCover` |
| `crop1..20.yuv` 命名 | `CenterCropGenYuvs` | `DecDemo::ReplaceAlhpaMergeToFovSmoothLp` |
| `cropN.yuv` 的 FOV：crop1 最窄、crop20 最宽 | `CenterCropGenYuvs` | `Replace` 里逆序构造 → 播放顺序宽 → 窄 → 封面 |
| `g_hMatrixVidToCover` 全局状态 | `RegCoverFrameToLastVidFrame` | `DumpXYScaleFroMatrixVidToCover` |
| 工作目录下有 `"60.yuv"` | `DecodeCoverMp4ToYuvs` | `DecodeEncodeAndInsertSingleYuv`（废弃路径） |
| `TransFrameNum = 20` 且 `[276, 295]` 长度 = 20 | `FFmpegDefine.h` | `DecodeAndEncodeAndReplaceMultiYuvs`（**无校验**） |

### 🟡 `cropRatio` 语义三处不一致

| 函数 | 参数语义 | 单位 |
| :--- | :--- | :--- |
| `CenterCropGenYuvs` | **单边**裁剪比例 | `[0, 0.5]` |
| `DecodeCropEncode` | **总**裁剪比例 | `[0, 1)` |
| `YUVProcessor::cropAndResize` | **每边**裁剪百分比 | `(0, 100)` |

**调用时要仔细确认单位**。

### 🔴 已知 bug / 隐患

| 位置 | 问题 | 影响 |
| :--- | :--- | :--- |
| `YUVImageReader::readYUV420PChannels` | 返回的 `cv::Mat` 引用局部 `vector` → use-after-free | 一调用就炸 |
| `DecodeAndEncodeAndInsertMultiYuvs` | `AVFramePtr` 接管后手动 `av_frame_free` → **double free** | 崩溃 |
| `DecodeEncode` / `InsertSingleYuv` / `InsertMultiYuvs` | 硬编码 400 kbps | 画质极差 |
| `YUVImageReader` 色彩范围 | full-range in/out，FFmpeg 编码 default limited-range | 亮度偏移 ~10% |
| `CROP_COVER_DIR` | 路径拼接双斜杠 `LivePhotoSmoothPlay//CropCover` | Windows 无害，跨平台可能报错 |
| `DecodeCropEncodeInputVid` | `1562` 硬编码与 `YUV_H = 1584` 不一致 | 裁剪结果与下游不匹配 |
| `ReplaceAlhpaMergeToFovSmoothAndAlpaMergedLp` | 与 `ReplaceAlhpaMergeToFovSmoothLp` 逐字相同 | 死代码 |
| `YUVProcessor::processPlane` | `y1` 上界误用 `crop_x + crop_width` | 当前不触发，潜在定时炸弹 |

### 🟢 设计亮点

| 项 | 评价 |
| :--- | :--- |
| 渐进式演进 | 从"裸指针 + 400kbps" → "完整 RAII + libx264 CRF20"，代码质量在演进中提升 |
| 大量调试产物 | 配准一次生成 10 个文件，人眼可验证质量 |
| `RegistrationResult` 聚合 | `num_inliers` / `rms_error` 提供质量指标 |
| ORB 5000 个特征点 | 对 FOV 差异大的场景是必要的 |
| 逆矩阵处理正确 | `warp_to_ref` 方向明确，注释清晰 |
| `函数内 static` 路径 | 已修复静态初始化顺序问题 |
| RAII 删除器统一 | `AVFormatContextPtr` 等 6 个类型别名统一使用 |

---

## 扩展指引

| 想做的 | 需要改 |
| :--- | :--- |
| 支持任意视频 | 把 `LAST_VID_FRAME_IDX` 从 `constexpr` 改为运行时配置（JSON 或命令行） |
| 自动检测最后视频帧 | 用帧间差异 / PTS 分析替代人工识别 |
| 支持任意分辨率 | `YUV_W`/`YUV_H` 从 `yuvWidthHeight.txt` 运行时读取 |
| 提高配准精度 | `ImageRegistrator` 加 SIFT（主库可用）；`Orb 5000` → 可调 |
| 消除色彩偏移 | 统一 `YUVImageReader` 和 FFmpeg 的 range |
| 支持批量处理 | 把 `g_hMatrixVidToCover` 全局改为函数返回；`current_path()` 改为显式路径 |
| 清理死代码 | 删 `GenMp4.h`、`DecodeEncode`、`InsertSingleYuv`、`InsertMultiYuvs`、`YUVProcessor::ConvertCoverFrameToMultiFovYuvs` |

---

## 相关文档

- **实现总览**：[`../../src/LivePhotoSmoothPlay/README.md`](../../src/LivePhotoSmoothPlay/README.md)
- **实现细节**：[`../../src/LivePhotoSmoothPlay/IMPLEMENTATION_DETAILS.md`](../../src/LivePhotoSmoothPlay/IMPLEMENTATION_DETAILS.md)
- **测试说明**：[`../../tests/LivePhotoSmoothPlay/README.md`](../../tests/LivePhotoSmoothPlay/README.md)
- **配置说明**：[`../../configs/README.md`](../../configs/README.md)