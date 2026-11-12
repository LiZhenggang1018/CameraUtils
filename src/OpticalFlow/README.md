# src/OpticalFlow —— 光流与插帧模块（实现总览）

## 概述

本目录是 `inc/OpticalFlow/` 的具体实现。本文档给出**整体结构、数据流、性能分析、调试方法**，
每个 `.cpp` 的**详细实现细节**（关键代码、算法流程、历史修复）见同目录的
[`IMPLEMENTATION_DETAILS.md`](./IMPLEMENTATION_DETAILS.md)。

**模块定位**：为上层 Demo（`tests/OpticalFlowTest.h`）和未来生产代码提供
"输入两帧 → 输出中间帧 / 稠密光流场 / MP4 视频" 的完整工具链。

**依赖**：OpenCV 4.10（`core/imgproc/video/optflow/ximgproc`）、FFmpeg（libx264 编码）。

---

## 文件结构总览

| 文件 | 对应头文件 | 行数（约） | 职责 |
| :--- | :--- | :---: | :--- |
| `FrameInterpolator.cpp` | `FrameInterpolator.h` | ~600 | 光流计算 + 融合（最复杂，核心） |
| `FfmpegVideoWriter.cpp` | `FfmpegVideoWriter.h` | ~330 | libx264 编码 + MP4 封装 |
| `LkTracker.cpp` | `LkTracker.h` | ~200 | 稀疏 LK 全流程 |
| `BiDirectionalLkFilter.cpp` | `BiDirectionalLkFilter.h` | ~100 | 双向 LK 一致性 |
| `MiddleburyColor.cpp` | `MiddleburyColor.h` | ~40 | 光流配色 |
| `Visualization.cpp` | `Visualization.h` | ~50 | 绘制工具 |

**复杂度分布**：`FrameInterpolator.cpp` 一个文件占了全部逻辑的 60%+，
所有光流后端、融合策略、性能开关都集中在这里。

---

## 模块划分

```
┌─────────────────────────────────────────────────────────────┐
│ 稀疏光流（LK 系）                                             │
│   LkTracker  ──持有──▶  BiDirectionalLkFilter                │
├─────────────────────────────────────────────────────────────┤
│ 稠密光流（三种后端，header-only）                              │
│   DenseFlowFarneback   ── 通用，平滑，慢                      │
│   DenseFlowDis         ── 快，生产默认                        │
│   DenseFlowDeepFlow    ── 慢，已弃用                          │
├─────────────────────────────────────────────────────────────┤
│ 视频插帧（核心）                                              │
│   InterpolationConfig  ──供给──▶  FrameInterpolator           │
│                                       │                      │
│                                       ├── 双向光流            │
│                                       ├── 一致性掩膜          │
│                                       ├── 静态区保护          │
│                                       └── 拉普拉斯融合        │
├─────────────────────────────────────────────────────────────┤
│ 可视化                                                        │
│   MiddleburyColor（稠密）  +  Visualization（稀疏）            │
├─────────────────────────────────────────────────────────────┤
│ 输出                                                          │
│   FfmpegVideoWriter  ── BGR 序列 → H.264 MP4                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 核心数据流

```
[输入]
  cv::VideoCapture.read() → cv::Mat BGR (CV_8UC3)

[光流计算 - ComputeFlows]
  ① 灰度化：cvtColor(BGR → GRAY)
  ② 双边预处理（可选）：bilateralFilter
  ③ 双向光流：
       ComputeFlow(prevGray, currGray) → flowFwd
       ComputeFlow(currGray, prevGray) → flowBwd
       ├── 降分辨率（可选）：resize → ComputeFlowInternal → resize → ×(1/s)
       └── ComputeFlowInternal 内部：
             - 大位移处理（DIS 时）：Farneback 粗算 → 上采样 → 作为初值
             - 后端分派：Farneback / DIS / TV-L1
             - TV-L1：setUseInitialFlow(false) + flow.release() + CleanFlowNaN
  ④ 光流后处理（可选）：PostProcessFlow（中值 + guidedFilter）
  ⑤ 时域平滑（可选）：EMA（只在 |F| < temporalMagThreshold 的区域）

[融合 - BlendFromFlows]
  ⑥ WarpToMid × 2 → warpedPrev, warpedCurr
  ⑦ ComputeConfidence → confidence
  ⑧ ComputeStaticMask → staticMask
  ⑨ 三路候选：candidateWarped, candidateLinear, candidateOccluded
  ⑩ 三区融合 → result32F
  ⑪ LaplacianBlend（可选）→ result8u

[输出]
  FfmpegVideoWriter::WriteFrame(result8u)
  → sws_scale(BGR → YUV420P)
  → avcodec_send_frame → avcodec_receive_packet
  → av_interleaved_write_frame → mp4 文件
```

---

## 性能瓶颈分析

### 各阶段耗时（1080p，Level 2）

| 阶段 | 耗时/对帧 | 优化开关 |
| :--- | :---: | :--- |
| **guidedFilter × 4（PostProcessFlow 内部）** | **200~400 ms** | `enableFlowPostProcess` |
| DIS FAST 光流 × 2 | ~140 ms | `disPreset`, `flowDownscale` |
| bilateralFilter × 2 | ~60 ms | `enableBilateralPreprocess` |
| 融合（各种 mul/add） | ~100 ms | — |
| 编码（libx264） | ~40 ms | `preset`, `crf` |
| LaplacianBlend | ~30 ms | `enableLaplacianBlend`, `laplacianLevels` |
| 时域平滑 | ~15 ms | `enableTemporalSmooth` |
| **总计** | **~500~800 ms** | — |

### 优化优先级（从高到低）

| 优先级 | 优化项 | 预期省时/对帧 | 画质损失 |
| :---: | :--- | :---: | :--- |
| **1** | `flowDownscale` 1.0 → 0.5 | ~70 ms | 微小 |
| **2** | `flowDownscale` 1.0 → 0.25 | ~105 ms | 中等 |
| **3** | `enableFlowPostProcess = false` | **200~400 ms** | 小 |
| **4** | `disPreset` 2 → 1 → 0 | ~30~50 ms | 小 |
| **5** | `enableBilateralPreprocess = false` | ~60 ms | 小 |
| **6** | `enableLaplacianBlend = false` | ~30 ms | 中 |
| **7** | `enableTemporalSmooth = false` | ~15 ms | 小 |

**理论上限**：Level 0 配置下约 **40~50 ms/对帧**（264 帧约 10 秒）。

### 性能档位（由 `InterpolationConfig::performanceLevel` 控制）

| 档位 | 每对帧耗时 | 264 帧总耗时 |
| :---: | :---: | :---: |
| 0（极速） | ~40 ms | ~10 秒 |
| 1（快） | ~80 ms | ~20 秒 |
| 2（平衡） | ~180 ms | ~45 秒 |
| 3（画质） | ~270 ms | ~70 秒 |
| 4（TV-L1） | ~400+ ms | ~2~3 分钟 |

---

## 调试与验证

### 日志关键点

启动时：

```text
[InterpolationConfig] Loaded: ... (factor=2, performanceLevel=2, flowBackend=1, flowDownscale=1.00)
========== UpsampleVideo begin ==========
[Config] factor=2, preset=veryfast, crf=20, bitrate=16000000
[Input ] .../1.mp4
[Meta ] W=1920 H=1088 srcFps=30.000 totalFrames=264
[Writer] opened, encoder=libx264, 1920x1088 @ 60fps
[Read #0] ret=1 empty=0 size=1920x1088 type=16 channels=3
```

结束：

```text
[Done] read=264 write=527
[libx264 ...] frame I:... P:... B:...
========== UpsampleVideo end ==========
```

**正确性检查**：`factor=2` 时输出帧数应为 `N*2-1`（264 → 527）。

### 常见异常日志

| 日志 | 含义 | 处理 |
| :--- | :--- | :--- |
| `[TV-L1] exception: ...` | TV-L1 内部异常 | 回退到零光流；考虑换 DIS |
| `[TV-L1] invalid output` | TV-L1 输出尺寸/类型不对 | 同上 |
| `[LaplacianBlend] exception: ...` | 拉普拉斯融合异常 | 回退到粗融合；检查 `laplacianLevels` 与 `buildPyramid` 层数一致性 |
| `[BUG] prev and curr share the same memory!` | `prev = curr` 浅拷贝 | 改用 `std::swap(prev, curr)` |

### 逐帧验证画质

用 PotPlayer 按 `F` 逐帧：

| 场景 | 期望 |
| :--- | :--- |
| 静止画面 | 中间帧与前后帧完全相同（`staticMask` 生效） |
| 慢速运动 | 中间帧介于前后帧之间，无明显重影 |
| 快速运动 | 可能轻微 ghost，但不应"大面积双影" |
| 文本/边缘 | 保持清晰（`LaplacianBlend` 高频用 warp） |

### 快速定位崩溃

**Release 下调试崩溃**：

1. VS 顶部构建配置选 `x64-release`
2. `调试 → 开始调试`（F5）
3. 崩溃弹窗点 **"中断"**（不要点"继续"）
4. `Ctrl+Alt+C` 打开调用堆栈 → 右键"显示外部代码"
5. 找最上面属于 `CameraUtils.exe` 的帧，双击跳到源码

**调用栈特征 → 原因对照**：

| 栈顶 | 原因 |
| :--- | :--- |
| `cv::multiply` → `AutoBuffer::allocate` | NaN/Inf 传播或尺寸不匹配 |
| `bilateralFilterInvoker_8u` | src/dst 共享内存 |
| `cv::pyrUp_` assertion | 金字塔层数不一致 |
| `msvcp140.dll` + `nullptr` | 全局静态初始化顺序（改"函数内 static"） |

---

## 扩展指引

| 想做的 | 需要改 |
| :--- | :--- |
| 增加新光流后端（如 RAFT） | `Params::flowBackend` 加枚举 + `ComputeFlowInternal` 加分支 + `CleanFlowNaN` 兜底 |
| 增加新融合策略 | `BlendFromFlows` 里加候选 + 调整最终权重 |
| 引入 RIFE（深度学习） | 新增 `RifeInterpolator` 类，`UpsampleVideo` 里按配置切换 |
| 支持多倍率 | 已在 `UpsampleVideo` 里实现，`factor` 改配置即可 |
| 支持音频复制 | `UpsampleVideo` 里 `av_stream_copy` |
| 支持 HDR | `FfmpegVideoWriter` 换 `AV_PIX_FMT_YUV420P10LE` + `AVCOL_TRC_SMPTE2084` |
| 管线并行 | 读/算/写三线程，`FrameInterpolator` 需要线程安全化（当前 `m_prevFlowFwd` 有状态） |

---

## 详细实现文档

**每个 `.cpp` 的详细实现细节请见** → [`IMPLEMENTATION_DETAILS.md`](./IMPLEMENTATION_DETAILS.md)

包含内容：
- `FrameInterpolator.cpp`：内部结构图、关键函数逐行解读、性能开关实现、历史修复
- `FfmpegVideoWriter.cpp`：Open/WriteFrame/Close 完整流程、EAGAIN 处理、BT.709 色彩管理
- `LkTracker.cpp`：索引一致性保证、Replenish 去重逻辑、双分支跟踪
- `BiDirectionalLkFilter.cpp`：四步过滤算法、索引对齐
- `MiddleburyColor.cpp` / `Visualization.cpp`：配色与绘制的实现要点
- 历史修复记录汇总表