# inc/OpticalFlow —— 光流与插帧模块（头文件层）

## 概述

本目录提供**光流估计**、**视频插帧**、**光流可视化**、**MP4 编码输出**四类能力，
所有头文件仅暴露接口，具体实现位于 `src/OpticalFlow/`。

**模块定位**：为上层 Demo（`tests/OpticalFlowTest.h`）和未来生产代码提供
"输入两帧 → 输出中间帧 / 稠密光流场 / MP4 视频" 的完整工具链。

**依赖**：OpenCV 4.10（`core/imgproc/video/optflow/ximgproc`）、FFmpeg（libx264 编码）。

---

## 文件结构总览

| 文件 | 类型 | 职责 |
| :--- | :--- | :--- |
| `FrameInterpolator.h` | 类声明 | **插帧核心**：双向光流 + 一致性掩膜 + 拉普拉斯融合 |
| `InterpolationConfig.h` | 结构体 + JSON 序列化 | **配置中枢**：所有可调参数，支持 `performanceLevel` 一键切换 |
| `LkTracker.h` | 类声明 | 稀疏 LK 跟踪器（角点检测 + 光流跟踪 + FB 过滤 + 补点） |
| `BiDirectionalLkFilter.h` | 类声明 | 双向 LK 一致性过滤器（剔除误匹配） |
| `DenseFlow.h` | header-only | Farneback 稠密光流 |
| `DenseFlowDis.h` | header-only | DIS 稠密光流（`cv::DISOpticalFlow`，video 模块） |
| `DenseFlowDeepFlow.h` | header-only | DeepFlow 稠密光流（`cv::optflow::createOptFlow_DeepFlow`） |
| `MiddleburyColor.h` | 函数声明 | Middlebury 标准光流配色（`FlowToColor`） |
| `Visualization.h` | 函数声明 | 绘制工具（轨迹、箭头） |
| `FfmpegVideoWriter.h` | 类声明 | **MP4 写出器**：BGR → YUV420P → libx264 编码 |

---

## 模块划分

### A. 稀疏光流（LK 系）

```
LkTracker  ──持有──▶  BiDirectionalLkFilter
```

- `LkTracker` 对外提供完整的"角点 → 跟踪 → 过滤 → 补点"流程
- `BiDirectionalLkFilter` 是被内部使用的组件，一般不直接调用

**典型用途**：跟踪若干特征点、可视化轨迹、目标跟踪。

### B. 稠密光流（三种后端）

```
DenseFlowFarneback   ── 通用，平滑，慢
DenseFlowDis         ── 快，精度中，生产默认
DenseFlowDeepFlow    ── 慢，质量中，已弃用（保留仅作对比）
```

三个类共享同一接口约定 `cv::Mat Compute(prevGray, currGray)`。

### C. 视频插帧（核心生产模块）

```
InterpolationConfig  ──供给──▶  FrameInterpolator
                                      │
                                      ├──（内部）双向光流
                                      ├──（内部）一致性掩膜
                                      ├──（内部）静态区保护
                                      └──（内部）拉普拉斯融合
```

**典型用途**：30fps → 60/90/120fps 视频流畅化。

### D. 可视化

```
MiddleburyColor（FlowToColor）    ── 稠密光流 → BGR 彩色图
Visualization（DrawLkTrails）      ── 稀疏光流轨迹
```

### E. 输出

```
FfmpegVideoWriter  ── BGR cv::Mat 序列 → H.264 MP4
```

不依赖 OpenCV 的 `cv::VideoWriter`（绕开 `opencv_videoio_ffmpeg*.dll` 依赖问题）。

---

## 每个文件的详细说明

### `FrameInterpolator.h` —— 插帧核心

#### 设计

将插帧拆成**光流计算**和**融合**两个阶段，允许多个中间帧复用同一次光流：

```
ComputeFlows(prev, curr, fF, fB)        ← 只算一次
    │
    ├── BlendFromFlows(prev, curr, fF, fB, t=1/3)   ← 复用
    ├── BlendFromFlows(prev, curr, fF, fB, t=2/3)   ← 复用
    └── BlendFromFlows(prev, curr, fF, fB, t=0.5)   ← 复用
```

这就是 3× 插帧不会慢 3 倍的原理。

#### 公开接口

```cpp
struct FrameInterpolator::Params {
    // 光流后端
    int    flowBackend          = 1;      // 0=Farneback, 1=DIS, 2=TV-L1
    int    disPreset            = 0;      // 0=ULTRAFAST, 1=FAST, 2=MEDIUM
    int    maxCoarseLevels      = 1;      // DIS 大位移处理层数

    // 性能开关
    double flowDownscale            = 1.0;    // 光流分辨率（0.25/0.5/1.0）
    bool   enableBilateralPreprocess = true;  // 光流前置双边滤波
    bool   enableFlowPostProcess     = true;  // 光流后处理（guidedFilter，最慢）
    bool   enableLaplacianBlend      = true;  // 拉普拉斯金字塔融合
    int    laplacianLevels           = 4;

    // 质量参数
    double fbThreshold          = 1.0;    // 前后向一致性阈值
    bool   enableStaticProtect  = true;
    double staticMagThreshold   = 1.5;
    double confidenceBlurSigma  = 3.0;
    double occlusionBlurSigma   = 2.0;

    // 时域
    bool   enableTemporalSmooth = true;
    double temporalAlpha        = 0.85;
    double temporalMagThreshold = 5.0;
};

// 一站式：内部调用 ComputeFlows + BlendFromFlows
cv::Mat Interpolate(prev, curr, t = 0.5);

// 两步拆分（多倍率插帧时优先用这个）
void ComputeFlows(prev, curr, flowFwd, flowBwd);
cv::Mat BlendFromFlows(prev, curr, flowFwd, flowBwd, t);
```

#### 内部流程（BlendFromFlows）

```
1. WarpToMid(prev, flowFwd, t)          → warpedPrev
2. WarpToMid(curr, flowBwd, -(1-t))     → warpedCurr
3. ComputeConfidence(fwd, bwd)           → confidence（前后向一致性）
4. ComputeStaticMask(fwd)                → staticMask（静态区）
5. 三路候选：
     candidateWarped   = (1-t)*wp + t*wc         ← 光流加权
     candidateLinear   = (1-t)*prev + t*curr     ← 线性混合
     candidateOccluded = occW*linear + (1-occW)*warpedAvg
6. 三区融合：
     nonStatic = conf*warped + (1-conf)*occluded
     result    = static*linear + (1-static)*nonStatic
7. LaplacianBlend（可选）              → 频段分离融合
```

#### 使用示例

```cpp
FrameInterpolator::Params params;
params.flowBackend = 1;
params.flowDownscale = 0.5;
FrameInterpolator interp(params);

cv::Mat fF, fB;
interp.ComputeFlows(prev, curr, fF, fB);
for (int k = 1; k < factor; ++k) {
    double t = static_cast<double>(k) / factor;
    cv::Mat mid = interp.BlendFromFlows(prev, curr, fF, fB, t);
    writer.WriteFrame(mid);
}
```

#### 注意事项

- **状态性**：`FrameInterpolator` 内部缓存 `m_prevFlowFwd`（时域平滑），**同一个实例要跨循环复用**，不要每对帧新建。
- **线程安全**：`ComputeFlows` 内部修改 `m_prevFlowFwd`，**同一实例不能多线程并发调用**。
- **`t` 范围**：`Interpolate(prev, curr, t)` 要求 `0 < t < 1`，`t = 0/1` 会被 `CV_Assert` 拒绝。
- **输入要求**：`prev` 和 `curr` 必须同尺寸、同类型（`CV_8UC3` 或 `CV_8UC1`）。

---

### `InterpolationConfig.h` —— 配置中枢

#### 设计

- 所有可调参数集中在一个 POD 结构体
- 通过 `NLOHMANN_DEFINE_TYPE_INTRUSIVE` 一行宏支持 JSON 序列化/反序列化
- `performanceLevel` 提供 5 个预设档位，一键切换
- `ApplyPerformanceLevel()` 在 `LoadFromFile` 时被调用，把档位展开成细粒度字段

#### 公开接口

```cpp
struct InterpolationConfig {
    int    performanceLevel = 2;   // 0~4 预设；-1 表示不覆盖细粒度字段
    int    factor           = 2;   // 插帧倍率
    // ...（编码、光流、质量、时域等，见源码）

    static const std::string& DefaultPath();
    static InterpolationConfig LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;
    void ApplyPerformanceLevel();   // 由 LoadFromFile 内部调用
};
```

#### 性能档位

| 值 | 耗时 | 使用参数组合 |
| :---: | :---: | :--- |
| 0 | ~10s | DIS ULTRAFAST + 1/4 分辨率 + 全关 |
| 1 | ~20s | DIS ULTRAFAST + 1/2 分辨率 + 关后处理 |
| 2 | ~30s | DIS FAST + 全分辨率 + 全开 |
| 3 | ~70s | DIS MEDIUM + 大位移 + 全开 |
| 4 | 2~3min | TV-L1 + 1/2 分辨率 + 全开 |

#### 注意事项

- **JSON 编码**：文件必须保存为 **UTF-8 无 BOM**，否则 `nlohmann/json` 解析失败会静默回退到默认值
- **首次运行**：`LoadFromFile` 发现文件不存在会自动 `Srlz` 生成默认配置
- **档位优先级**：`performanceLevel >= 0` 时**覆盖**细粒度字段；`-1` 时**不覆盖**

---

### `LkTracker.h` —— 稀疏光流跟踪器

#### 公开接口

```cpp
struct LkTracker::Params {
    int    maxCorners         = 500;
    double qualityLevel       = 0.01;
    double minDistance        = 10.0;
    int    blockSize          = 3;
    bool   useHarrisDetector  = false;
    double k                  = 0.04;

    cv::Size winSize          = {21, 21};
    int    maxLevel           = 3;
    cv::TermCriteria criteria = {COUNT|EPS, 30, 0.01};
    double replenishRatio     = 0.5;

    bool   enableFbFilter     = true;
    double fbThreshold        = 1.5;
};

void Detect(const cv::Mat& gray);
void Track(const cv::Mat& prevGray, const cv::Mat& gray);
void AddPoint(const cv::Point2f& point);
bool RemovePoint(const cv::Point2f& point, double radius = 8.0);
void Reset();

const std::vector<cv::Point2f>& Points() const;
const std::vector<cv::Point2f>& PrevPoints() const;
int TrackedCount() const;

void SetMaxLevel(int level);
void SetFbThreshold(double threshold);
void SetEnableFbFilter(bool enable);
double AverageFbError() const;
```

#### 内部流程

```
Detect:  goodFeaturesToTrack → m_currPts → m_prevPts = m_currPts
Track:   m_currPts 空 → 调 Detect
         否则 → (开 FB) m_fbFilter.Filter → goodPrev/goodCurr
                (关 FB) calcOpticalFlowPyrLK → 手动筛 status
         m_prevPts = goodPrev; m_currPts = goodCurr
         点数 < maxCorners * replenishRatio → Replenish(gray)
```

#### 注意事项

- **索引一致性**：所有路径（`Detect/Track/Replenish/AddPoint/RemovePoint`）都保证 `m_prevPts.size() == m_currPts.size()`，下游 `DrawLkTrails` 依赖此不变量
- **`Replenish` 可能超过 `maxCorners`**：传的是 `maxCorners` 而不是剩余配额，补完后可能超过 500 个点
- **`Track` 时 `m_currPts` 为空会调 `Detect`**：低纹理素材会每帧重检测，可考虑加降频重试

---

### `BiDirectionalLkFilter.h` —— 双向 LK 一致性过滤

#### 公开接口

```cpp
void Filter(
    const cv::Mat& prevGray,
    const cv::Mat& gray,
    const std::vector<cv::Point2f>& prevPts,
    std::vector<cv::Point2f>& outPrevPts,   // 输出：通过验证的 prev 侧点
    std::vector<cv::Point2f>& outCurrPts,   // 输出：对应的 curr 侧点
    std::vector<float>& outFbErrors) const; // 输出：每个点的 FB 误差
```

#### 算法

```
1. 前向 LK：prevGray → gray，计算 forwardPts/status
2. 筛前向成功的点：validPrev, validForward
3. 反向 LK：gray → prevGray，从 validForward 反推 backwardPts
4. FB 误差 = ||validPrev[i] - backwardPts[i]||
5. 误差 < fbThreshold → 保留到 outPrevPts/outCurrPts/outFbErrors
```

#### 注意事项

- **输出语义**：`outFbErrors` 只包含"通过验证的点"的误差，前向就失败的点不计入
- **索引对齐**：`validPrev[i]` ↔ `validForward[i]` ↔ `backwardPts[i]` 一一对应
- 两次 `calcOpticalFlowPyrLK` 各建一次金字塔，**有性能优化空间**（OpenCV 高层 API 未暴露复用接口）

---

### `DenseFlow.h` / `DenseFlowDis.h` / `DenseFlowDeepFlow.h` —— 稠密光流

三者共享接口：

```cpp
cv::Mat Compute(const cv::Mat& prevGray, const cv::Mat& gray) const;
```

| 类 | 后端 | 依赖模块 | 特点 |
| :--- | :--- | :--- | :--- |
| `DenseFlowFarneback` | `cv::calcOpticalFlowFarneback` | imgproc | 平滑，慢，精度中 |
| `DenseFlowDis` | `cv::DISOpticalFlow` | **video** | 快，精度中高，**生产默认** |
| `DenseFlowDeepFlow` | `cv::optflow::createOptFlow_DeepFlow` | optflow | 慢，边缘较好，已弃用 |

`DenseFlowDis` 额外提供：

```cpp
enum class Preset { UltraFast, Fast, Medium };
void SetPreset(Preset p);
void CyclePreset();          // 用于交互式切换
const char* PresetName() const;
```

#### 注意事项

- **`DISOpticalFlow` 在 `video` 模块，不在 `optflow`** —— 这是 OpenCV 4.x 相对 3.x 的模块归属变更，常见的编译错误是 `cv::optflow::DISOpticalFlow` 找不到
- **`DenseFlowDeepFlow` 每次构造都要建 DIS/DeepFlow 实例**，不要放在高频循环里构造
- 三者的 `Compute` 都要求输入为 **`CV_8UC1` 单通道灰度图**

---

### `MiddleburyColor.h` —— 光流可视化

#### 公开接口

```cpp
cv::Mat FlowToColor(const cv::Mat& flow);   // CV_32FC2 → CV_8UC3 (BGR)
```

#### 算法

```
1. split → (dx, dy)
2. cartToPolar(dx, dy) → magnitude, angle（角度制 0~360）
3. minMaxLoc 归一化 magnitude 到 [0,1]
4. HSV: H = angle * 0.5（OpenCV 8bit H 范围 0~179，所以 360° / 2）
        S = 255
        V = magnitude * 255
5. HSV2BGR
```

#### 注意事项

- **逐帧归一化**：每帧按当前帧的 `magMax` 归一化，帧间亮度不可直接对比，对视觉观察友好
- `angle * 0.5` 是 OpenCV 特有的 H 通道范围约定，不能改成 `angle` 或 `angle * 360/179`

---

### `Visualization.h` —— 绘制工具

#### 公开接口

```cpp
void DrawLkTrails(
    cv::Mat& image,
    const std::vector<cv::Point2f>& prevPts,
    const std::vector<cv::Point2f>& currPts,
    const cv::Scalar& color = cv::Scalar(0, 255, 0));

void DrawDenseFlowArrows(
    cv::Mat& image,
    const cv::Mat& flow,
    int step = 16,
    double scale = 1.0,
    const cv::Scalar& color = cv::Scalar(0, 255, 0));
```

#### 注意事项

- **`DrawLkTrails` 隐含假设** `prevPts.size() == currPts.size()`，从 `LkTracker` 拿到的向量天然满足，但**别的地方调用时要自己保证**
- `DrawDenseFlowArrows` 在 1080p + `step=16` 下每帧约 8000 个箭头，性能开销较大，当前 Demo 未使用（改用 `FlowToColor`）

---

### `FfmpegVideoWriter.h` —— MP4 写出器

#### 公开接口

```cpp
struct EncoderParams {
    std::string preset = "veryfast";
    int         crf    = -1;   // >= 0 用 CRF；-1 用 ABR
};

bool Open(const std::string& path, int width, int height,
          int fps, int bitrate = 8000000,
          const EncoderParams& enc = EncoderParams{});

bool WriteFrame(const cv::Mat& bgr);   // CV_8UC3
void Close();

bool IsOpened() const;
const char* EncoderName() const;
```

#### 内部流程

```
Open:
  avformat_alloc_output_context2 → avcodec_find_encoder("libx264")
  → avcodec_alloc_context3 → 参数设置（BT.709 色彩 metadata）
  → avcodec_open2（preset/crf 通过 AVDictionary 传）
  → avformat_new_stream → avio_open → avformat_write_header
  → sws_getContext（BGR24 → YUV420P，BT.709）
  → av_frame_alloc + av_frame_get_buffer

WriteFrame:
  sws_scale(BGR → YUV420P)
  → avcodec_send_frame → avcodec_receive_packet 循环
  → av_packet_rescale_ts → av_interleaved_write_frame

Close:
  avcodec_send_frame(nullptr) 触发 flush
  → av_write_trailer（关键！moov atom 在这里）
  → avio_closep + 释放所有资源
```

#### 注意事项

- **必须调 `Close()`**：不调则 MP4 没有 `moov atom`，播放器/`ffprobe` 会报 `moov atom not found`
- **BT.709 色彩**：输入 BGR（full range, BT.709 源）→ 输出 YUV420P（limited range, BT.709），通过 `sws_setColorspaceDetails` + `AVCodecContext` metadata 双保险，**不设置会出现画面偏黄/偏红**
- **CRF 与 bitrate 互斥**：`crf >= 0` 时 `bit_rate` 被置 0；`crf = -1` 时用 `bitrate`（ABR）
- **`WriteFrame` 输入必须是 `CV_8UC3`**，否则会被拒绝

---

## 调用时序

### 时序 1：交互式 Demo（`OpticalFlowTest()`）

```
启动
 ├── 创建 cv::VideoCapture
 ├── 创建 LkTracker + 3 个 DenseFlow*
 ├── 创建 namedWindow + setMouseCallback
 └── 主循环：
      ├── [未暂停] 读帧 → cvtColor(灰度)
      │    ├── 首帧 → LkTracker::Detect
      │    └── 后续 → LkTracker::Track
      │         └── 若 mode 为 Dense/Dis/Overlay:
      │              ├── DenseFlowFarneback::Compute（或 Dis）
      │              ├── FlowToColor(flow) → 彩色图
      │              └── 计时记录
      ├── 若 mode 为 DeepFlow 且暂停且脏：
      │    └── DenseFlowDeepFlow::Compute → FlowToColor
      ├── 按 mode 选择 display：
      │    ├── Lk     → DrawLkTrails(m_currentFrame)
      │    ├── Dense  → denseColor
      │    ├── Dis    → denseColor
      │    ├── DeepFlow → denseColor
      │    └── Overlay → denseColor + DrawLkTrails
      ├── putText 状态栏
      ├── imshow + waitKey
      └── 按键处理（m/空格/r/f/b/B/+/-/w/W/p/q）
```

### 时序 2：批量插帧（`UpsampleVideo`）

```
1. InterpolationConfig::LoadFromFile(DefaultPath())
    ├── Dsrlz<InterpolationConfig>(path)  （不存在 → Srlz 生成默认 + 返回默认）
    └── ApplyPerformanceLevel() 展开 performanceLevel
2. 构造 FrameInterpolator::Params（从 cfg 拷贝）
3. 构造 FrameInterpolator interp(params)   ← 复用实例，保留时域状态
4. 构造 FfmpegVideoWriter::EncoderParams（preset / crf）
5. cv::VideoCapture(inputPath)
6. FfmpegVideoWriter writer.Open(outputPath, W, H, outFps, bitrate, enc)
7. cap.read(prev) → writer.WriteFrame(prev)    ← 第 0 帧
8. while (cap.read(curr)) {
       interp.ComputeFlows(prev, curr, fF, fB)   ← 一次
       for k = 1 .. factor-1:
           t = k/factor
           mid = interp.BlendFromFlows(prev, curr, fF, fB, t)  ← 复用
           writer.WriteFrame(mid)
       writer.WriteFrame(curr)                    ← 原始后帧
       std::swap(prev, curr)                       ← 关键！不能写 prev = curr
   }
9. writer.Close()                                 ← 必须调用，否则 MP4 无 moov atom
```

---

## 注意事项（高频坑）

### 1. `std::swap(prev, curr)` 不能用 `prev = curr` 替代

`cv::Mat` 的 `=` 是**浅拷贝**。写 `prev = curr` 后两者共享同一块内存，下次 `cap.read(curr)` 会覆盖共享内存，`prev` 也变成新帧内容 → **光流输入两张相同的图** → 输出全 0 光流 → 中间帧退化成 `prev`。

**验证**：在 `Interpolate` 里打印 `prev.data == curr.data`，正常应为 `false`。

### 2. `FfmpegVideoWriter` 必须 `Close()`

只在 `av_write_trailer` 写 `moov atom`。不调用会导致文件打不开（`ffprobe` 报 `moov atom not found`），**程序正常退出时代码里的 `~FfmpegVideoWriter` 会兜底调用 `Close()`**，但**异常退出时不会**。

### 3. 色彩空间必须显式设置 BT.709

Windows + MSMF 解码的 mp4 是 BT.709，`sws_scale` 默认按 BT.601 转，**不设置 `sws_setColorspaceDetails` 会导致画面偏黄**。已在 `FfmpegVideoWriter::Open` 里处理。

### 4. `DISOpticalFlow` 属于 `video` 模块，不是 `optflow`

`cv::optflow::DISOpticalFlow` 找不到 → 应该用 `cv::DISOpticalFlow`。这是 OpenCV 4.x 相对 3.x 的变更。

### 5. `LaplacianBlend` 的 `buildPyramid` 层数

`buildPyramid(src, pyr, levels - 1)` 而不是 `levels`。前者构建 `levels` 层（`pyr[0..levels-1]`），后者构建 `levels+1` 层，`pyr.back()` 的尺寸会和循环里最后一层对不上，`pyrUp` 断言失败。

### 6. `bilateralFilter` 的 src/dst 不能共享内存

`cv::Mat prevForFlow = prevGray;` 是浅拷贝，`bilateralFilter(prevGray, prevForFlow, ...)` 会因内部多线程读写冲突崩溃。**必须先声明空 Mat**：`cv::Mat prevForFlow;`。

### 7. `guidedFilter` 是最慢的后处理

`enableFlowPostProcess = true` 时每对帧多花 200~400ms。追求速度时**优先关闭这一项**。

### 8. 时域平滑依赖实例状态

`FrameInterpolator` 内部 `m_prevFlowFwd` 跨帧保留。**同一视频不要每帧新建 `FrameInterpolator`**，否则时域平滑失效。

---

## 扩展指引

| 想做的 | 需要改的地方 |
| :--- | :--- |
| 增加新的光流后端 | `FrameInterpolator::Params::flowBackend` 加枚举值 + `ComputeFlowInternal` 加分支 |
| 增加新的融合策略 | `BlendFromFlows` 里新增候选 + 在最后融合阶段加权重 |
| 支持多倍率插帧 | 已在 `UpsampleVideo` 里实现，改 `cfg.factor` 即可 |
| 支持 VFR | `FfmpegVideoWriter` 需要把 `pts` 从固定 `1/fps` 改为按真实时间戳 |
| 支持 HDR | `FfmpegVideoWriter` 换 `AV_PIX_FMT_YUV420P10LE` + `AVCOL_TRC_SMPTE2084` |
| 支持音频 | `UpsampleVideo` 里用 `av_stream_copy` 复制音频流 |