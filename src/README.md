# src/LivePhotoSmoothPlay —— LivePhoto 平滑播放（实现总览）

## 概述

本目录是 `inc/LivePhotoSmoothPlay/` 的 `.cpp` 实现。本文档给出**文件结构、核心数据流、性能分析、调试方法**，每个 `.cpp` 的**详细实现细节**见同目录的
[`IMPLEMENTATION_DETAILS.md`](./IMPLEMENTATION_DETAILS.md)。

**注意**：本模块 **13 个头文件里只有 2 个需要 `.cpp` 实现**，其余 11 个都是 `header-only`（全部 `inline`）。

---

## 文件结构总览

| 文件 | 对应头文件 | 行数（约） | 职责 |
| :--- | :--- | :---: | :--- |
| `ImageRegistrator.cpp` | `ImageRegistrator.h` | ~380 | 配准算法（ORB + HOMOGRAPHY + RANSAC） |
| `YUVImageReader.cpp` | `YUVImageReader.h` | ~130 | YUV420P ↔ BGR 读写 |

**这 2 个 `.cpp` 是整个模块的"算法核心"**——其他所有 `header-only` 文件都只是把这两个的能力组合成业务流程。

---

## 模块划分

```
┌─────────────────────────────────────────────────────────────┐
│ 业务编排层（header-only）                                     │
│   LpFovTransitionOpt.h  →  DecDemo.h                        │
├─────────────────────────────────────────────────────────────┤
│ 配准编排层（header-only）                                     │
│   LpReg.h                                                   │
├─────────────────────────────────────────────────────────────┤
│ 算法实现层（.cpp）                                            │
│   ImageRegistrator.cpp  ── ORB / HOMOGRAPHY / RANSAC        │
│   YUVImageReader.cpp    ── YUV ↔ BGR                        │
├─────────────────────────────────────────────────────────────┤
│ 工具层（header-only）                                         │
│   CenterCropGenYuvs.h  AlphaMergeYuvs.h  DumpUtils.h         │
│   FFMpegUtils.h  FFmpegDefine.h  YUVProcessor.h              │
├─────────────────────────────────────────────────────────────┤
│ FFmpeg 编解码层（header-only）                                │
│   DecAndEnc.h  GenMp4.h                                     │
└─────────────────────────────────────────────────────────────┘
```

**注意**：只有 `ImageRegistrator.cpp` 和 `YUVImageReader.cpp` 需要编译，其他 11 个头文件都是 `inline`——**修改它们不影响链接，但会影响所有 include 它们的翻译单元**。

---

## 核心数据流

```
Lp.mp4
  │
  ▼ FFmpeg 解码（DecAndEnc.h: DecodeMp4ToYuvFrames）
  │
LpJpg/{1.yuv, 2.yuv, ...}
  │
  ├──────────────────────────────────────────┐
  │                                          │
  ▼ 封面帧（COVER_FRAME_YUV）                  ▼ 最后视频帧（LAST_VID_FRAME_YUV）
  │                                          │
  │    ┌─────────────────────────────────────┘
  │    │
  ▼    ▼ YUVImageReader::readYUV420P
BGR  BGR
  │    │
  └────┴──────────────────────────────────┐
                                          │
                                          ▼ ImageRegistrator::registerImages
                                    RegistrationResult
                                     - transform_matrix (reference → target)
                                     - num_inliers / rms_error
                                          │
                                          ▼ 求逆 → warp_to_ref
                                    registered_image（target 对齐到 reference）
                                          │
                                          ▼ DumpXYScaleFroMatrixVidToCover
                                    vidToCoverScaleRatio.txt（x, y）
                                          │
                                          ▼ GenDiffFovYuvsFromCoverFrame
                                    ratio = (1 - avg(x,y)) / 2 / 20
                                          │
                                          ▼ CenterCropGenYuvs
                                    CropCover/{crop1..crop20}.yuv + .jpg
                                          │
                                          ▼ ReplaceAlhpaMergeToFovSmoothLp
                                    DecodeAndEncodeAndReplaceMultiYuvs
                                     - 解码 Lp.mp4
                                     - [276, 295] 替换为 crop20..crop1
                                     - libx264 CRF20 编码
                                          │
                                          ▼
                                    LpOpt.mp4
                                          │
                                          ▼ DecodeOptLpMp4ToYuvs
                                    LpOptJpg/{1.yuv, 2.yuv, ...}
```

---

## 性能分析

### 各阶段耗时（1080p 单次配准）

| 阶段 | 耗时（估算） | 优化开关 |
| :--- | :---: | :--- |
| FFmpeg 解码（264 帧） | ~2~5 s | — |
| **ORB 特征检测 × 2**（5000 个点） | **~60~100 ms** | `ImageRegistrator` 参数 |
| 特征匹配（BFMatcher Hamming） | ~30~50 ms | — |
| RANSAC 估计 H 矩阵 | ~5~10 ms | — |
| **`warpPerspective`** | **~10~20 ms** | `INTER_LINEAR` |
| RMS 误差计算 | ~1 ms | — |
| 保存 10 个调试文件 | ~200~500 ms | 减少调试产物 |
| CenterCropGenYuvs（20 帧） | ~100~200 ms | `INTER_LINEAR` |
| FFmpeg 编码（527 帧，CRF20） | ~10~20 s | `preset` |

**配准本身 ~100~200 ms，是整个流程里相对快的部分**——瓶颈在 FFmpeg 编解码。

### 一次性开销 vs 可重复开销

| 步骤 | 频率 | 说明 |
| :--- | :---: | :--- |
| 解码 Lp.mp4 | **每次处理都要** | 264 帧 × ~5ms = ~1.3s |
| 配准（封面 ↔ 视频帧） | **每个视频一次** | ~200ms |
| FOV 渐变生成 | **每个视频一次** | ~200ms |
| 替换 + 编码 | **每次处理都要** | 527 帧 × ~20ms = ~10s |
| 解码 LpOpt.mp4（验证） | **每次处理都要** | 527 帧 × ~5ms = ~2.6s |

**如果只处理一个视频**，总耗时约 **15~30 秒**；如果批量处理，中间的配准结果（`vidToCoverScaleRatio.txt` + `cropN.yuv`）**可以复用**。

---

## 调试与验证

### 日志关键点

启动时：

```text
=================================
YUV420P图像配准程序开始
=================================
配置参数:
---------------------------------
图像尺寸: 720x1584
参考图像: .../LpJpg/275.yuv
目标图像: .../LpJpg/296.yuv
输出目录: .../coverToVidRegResult
特征检测器: ORB
变换类型: 单应性变换
=================================
```

配准后：

```text
检测特征点...
参考图像特征点数: 5000
目标图像特征点数: 5000
匹配特征点...
匹配点数: 1523
估计变换矩阵...
内点数: 892
配准完成，耗时: 156ms
RMS误差: 1.23 像素
```

**关键指标**：

| 指标 | 期望值 | 异常判断 |
| :--- | :--- | :--- |
| `参考特征点数` | ≈ 5000（ORB 上限） | < 1000 → 输入图纹理不够 |
| `匹配点数` | 500~2000 | < 100 → 两图差异太大或配准方向错 |
| `内点数` | 200~1500 | < 50 → 配准不可靠，后续 FOV 渐变会错 |
| `RMS 误差` | 0.5~3.0 像素 | > 5 → 配准精度差 |

### 逐帧验证画质

用 PotPlayer 按 `F` 逐帧查看 `LpOpt.mp4`：

| 帧区间 | 期望 |
| :--- | :--- |
| `[1, 275]` | 原始视频内容不变 |
| `[276, 295]` | **20 帧 FOV 渐变**（从视频帧 FOV 平滑过渡到封面帧 FOV） |
| `[296]` | 封面帧（全幅） |

**如果 `[276, 295]` 区间"看不出渐变"**——可能是 `vidToCoverScaleRatio` 接近 1（配准没检测到缩放），或 `ratio` 太小（`TransFrameNum` 太大）。

**如果 `[276, 295]` 区间"跳跃感强"**——可能是 `ratio` 太大（`TransFrameNum` 太小），或者配准精度不够导致 FOV 计算错误。

### 调试产物

`RegCoverToVid()` 一次配准产出 10 个文件到 `REG_RESULT_DIR`：

| 文件 | 用途 |
| :--- | :--- |
| `01_reference_original.jpg` | 参考图（最后视频帧） |
| `02_target_original.jpg` | 目标图（封面帧） |
| `03_feature_matches.jpg` | 特征匹配可视化 |
| `04_overlay_result.jpg` | 半透明叠加 |
| `05_side_by_side.jpg` | 并排对比 |
| `06_difference.jpg` | 差异图 |
| `07_registered_image.yuv` | 配准后的 YUV |
| `08_registration_report.jpg` | 配准报告 |
| `09_configuration.txt` | 配置 |
| `transform_matrix.txt` | H 矩阵 |
| `matches.txt` | 匹配点详情 |

**优先看 `04_overlay_result.jpg`**——如果两张图叠加后"看不出两套边缘"，说明配准成功；如果"看到两套边缘"，说明配准失败。

### 快速定位崩溃

**Release 下调试崩溃**：

1. VS 顶部构建配置选 `x64-release`
2. `调试 → 开始调试`（F5）
3. 崩溃弹窗点 **"中断"**
4. `Ctrl+Alt+C` 打开调用堆栈 → 右键"显示外部代码"
5. 找最上面属于 `CameraUtils.exe` 的帧，双击跳到源码

**调用栈特征 → 原因对照**：

| 栈顶 | 原因 |
| :--- | :--- |
| `cv::split` / `cv::merge` → `AutoBuffer::allocate` | 输入 Mat 类型/尺寸不对 |
| `avcodec_send_frame` 返回负 | 编码器参数不匹配 |
| `fread` 返回 0 | YUV 文件大小不足 |
| `msvcp140.dll` + `nullptr` | 全局静态初始化顺序（已修复） |

---

## 扩展指引

| 想做的 | 需要改 |
| :--- | :--- |
| 自动检测最后视频帧 | `DecAndEnc.h` 的 `DecodeMp4ToYuvFrames` 加"帧间差异分析"，输出 `last_vid_idx` 到 metadata |
| 支持任意分辨率 | `YUV_W` / `YUV_H` 从 `yuvWidthHeight.txt` 运行时读 |
| 提高配准精度 | `ImageRegistrator::createDetector` 加 SIFT（主库可用）；ORB 5000 可调 |
| 消除色彩偏移 | `YUVImageReader` 用 `COLOR_BGR2YUV_I420`（limited-range），与 FFmpeg 统一 |
| 清理死代码 | 删 `GenMp4.h`、`DecodeEncode`、`InsertSingleYuv`、`InsertMultiYuvs`、`YUVProcessor::ConvertCoverFrameToMultiFovYuvs` |
| 批量处理 | `g_hMatrixVidToCover` 改为函数返回；`current_path()` 改为显式路径 |
| 减少调试产物 | 加 `enableDebugOutput` 开关，关闭时只保留 `transform_matrix.txt` |
| 提高编码质量 | `DecodeAndEncodeAndReplaceMultiYuvs` 的 `bit_rate = 4000000` 和 CRF 20 可调 |

---

## 详细实现文档

**每个 `.cpp` 的详细实现细节请见** → [`IMPLEMENTATION_DETAILS.md`](./IMPLEMENTATION_DETAILS.md)

包含内容：
- `ImageRegistrator.cpp`：ORB 5000 参数、BFMatcher/FLANN 分派、RANSAC 配置、逆矩阵处理、RMS 计算
- `YUVImageReader.cpp`：YUV ↔ BGR 转换流程、use-after-free bug、色彩范围问题
- 历史修复汇总