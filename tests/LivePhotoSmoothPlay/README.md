# tests/LivePhotoSmoothPlay —— LivePhoto 平滑播放测试

## 概述

本目录是 **LivePhoto 平滑播放**特性的端到端测试入口。

**测试目标**：验证"从原始 LP（`Lp.mp4`）生成优化 LP（`LpOpt.mp4`）"的完整流程——解码、配准、FOV 渐变生成、替换、重新编码。

**依赖**：
- 输入数据：`data/LivePhotoSmoothPlay/Lp.mp4`（原始 LP）
- 底层代码：`inc/LivePhotoSmoothPlay/LpFovTransitionOpt.h`
- 外部库：FFmpeg（libx264）、OpenCV

---

## 文件结构

| 文件 | 职责 |
| :--- | :--- |
| `LivePhotoSmoothPlayTest.h` | 测试入口，一行调用 `LpFovTransitionOptTest()` |

---

## 测试入口

### `LivePhotoSmoothPlayTest.h`

```cpp
#ifndef LIVEPHOTOSMOOTHPLAYTEST_H_
#define LIVEPHOTOSMOOTHPLAYTEST_H_

#include "LivePhotoSmoothPlay/LpFovTransitionOpt.h"

inline void RunLivePhotoSmoothPlayTest()
{
    LpFovTransitionOptTest();
}

#endif
```

**极简**——只有一行有效代码，直接调 `LpFovTransitionOptTest()`。

### 调用链

```
RunLivePhotoSmoothPlayTest()
  └─ LpFovTransitionOptTest()        ← LpFovTransitionOpt.h
       └─ GenFromLpToOptLp()         ← LpFovTransitionOpt.h
            ├─ DecodeLpMp4ToYuvs()
            ├─ RegCoverToVid()
            ├─ GenDiffFovYuvsFromCoverFrame()
            ├─ ReplaceAlhpaMergeToFovSmoothLp()
            └─ DecodeOptLpMp4ToYuvs()
```

---

## 如何运行

### 1. 前置准备

#### 1.1 输入文件

需要把 `Lp.mp4` 放到：

```text
<项目根>/data/LivePhotoSmoothPlay/Lp.mp4
```

**`Lp.mp4` 的要求**：

- **分辨率必须是 720×1584**（`YUV_W` / `YUV_H` 硬编码）
- **包含 alpha 混合过渡帧 + 封面帧**——即"原始 LP 结构"
- 视频帧数至少 `COVER_FRAME_IDX = 296`

#### 1.2 手动确认帧索引（关键步骤）

打开 `inc/LivePhotoSmoothPlay/FFmpegDefine.h`：

```cpp
inline constexpr int LAST_VID_FRAME_IDX = 275;   // ← 视频最后一帧
inline constexpr int TransFrameNum = 20;          // ← 过渡帧数
```

**`LAST_VID_FRAME_IDX` 需要手动改**：

1. 先运行一次 `DecodeLpMp4ToYuvs()`（或在 `GenFromLpToOptLp` 里暂时注释掉后续步骤）
2. 打开 `data/LivePhotoSmoothPlay/LpJpg/` 下的 `{N}.jpg`
3. **人眼找**"照片水印消失的那一帧"——即**最后一个视频帧**
4. 把这个 N 填到 `LAST_VID_FRAME_IDX`
5. 重编译

**`COVER_FRAME_IDX` 不用改**（自动 = `LAST_VID_FRAME_IDX + 21`），但要理解它的语义：

- `LAST_VID_FRAME_IDX + 1` 到 `LAST_VID_FRAME_IDX + 20` 是 alpha 混合过渡帧
- `LAST_VID_FRAME_IDX + 21` 是封面帧

#### 1.3 调用入口

在 `src/main.cpp` 里调用：

```cpp
#include "LivePhotoSmoothPlayTest.h"

int main() {
    RunLivePhotoSmoothPlayTest();
    return 0;
}
```

### 2. 构建

```cmd
cd /d C:\Users\11186484\CameraUtils
cmake --build --preset x64-release --target CameraUtils
```

### 3. 运行

```cmd
cd /d C:\Users\11186484\CameraUtils\out\build\x64-release\src
CameraUtils.exe
```

---

## 预期输出

### 控制台日志

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

步骤1: 初始化YUV读取器...
步骤2: 读取YUV图像文件...
图像读取成功!
参考图像尺寸: 720x1584, 通道数: 3
目标图像尺寸: 720x1584, 通道数: 3

步骤3: 保存原始图像用于对比...
步骤4: 开始图像配准...
检测特征点...
参考图像特征点数: 5000
目标图像特征点数: 5000
匹配特征点...
匹配点数: ~1500
估计变换矩阵...
内点数: ~900
配准完成，耗时: ~150ms
RMS误差: ~1.5 像素
...
```

### 磁盘产物

#### `data/LivePhotoSmoothPlay/LpJpg/`

```
1.yuv, 2.yuv, ..., N.yuv        ← 原始 LP 解码的 YUV
1.jpg, 2.jpg, ..., N.jpg        ← 对应 JPG（人眼验证用）
```

#### `data/LivePhotoSmoothPlay/coverToVidRegResult/`

```
01_reference_original.jpg        ← 最后视频帧
02_target_original.jpg           ← 封面帧
03_feature_matches.jpg           ← 特征匹配可视化
04_overlay_result.jpg            ← 半透明叠加（★ 优先看这个）
05_side_by_side.jpg              ← 并排对比
06_difference.jpg                ← 差异图
07_registered_image.yuv          ← 配准后的 YUV
08_registration_report.jpg       ← 配准报告
09_configuration.txt             ← 配置
transform_matrix.txt             ← H 矩阵
matches.txt                      ← 匹配点详情
```

#### `data/LivePhotoSmoothPlay/vidToCoverScaleRatio.txt`

```
<scaleX> <scaleY>
```

例：`0.94 0.94`——表示视频帧是封面帧的 94%。

#### `data/LivePhotoSmoothPlay/CropCover/`

```
crop1.yuv, crop2.yuv, ..., crop20.yuv   ← FOV 渐变过渡帧
crop1.jpg, ..., crop20.jpg              ← 对应 JPG
```

#### `data/LivePhotoSmoothPlay/LpOpt.mp4`

优化后的 LP 视频。

#### `data/LivePhotoSmoothPlay/LpOptJpg/`

```
1.yuv, 2.yuv, ..., N.yuv    ← LpOpt.mp4 的 YUV
```

---

## 验证方法

### 1. 配准是否成功

**优先看 `coverToVidRegResult/04_overlay_result.jpg`**：

- **成功**：半透明叠加后"看不出两套边缘"
- **失败**：能看到"两套边缘错位"

**辅助看 `coverToVidRegResult/transform_matrix.txt`**：

| 指标 | 期望 |
| :--- | :--- |
| `内点数` | > 200（可靠）；< 50（不可靠） |
| `RMS误差` | < 3.0 像素（精度好）；> 5.0 像素（精度差） |

### 2. FOV 渐变是否正确

用 PotPlayer 打开 `LpOpt.mp4`，按 `F` 逐帧：

| 帧区间 | 期望 |
| :--- | :--- |
| `[1, 275]` | 原始视频内容不变 |
| `[276, 295]` | **20 帧 FOV 渐变**（从视频帧 FOV 平滑过渡到封面帧 FOV） |
| `[296]` | 封面帧（全幅） |

**验证技巧**：找一个"有明显角点/纹理"的场景，逐帧对比 `[276, 295]` 区间的边缘位置——**应该能看到边缘逐渐向外扩展**（FOV 变大）。

**如果 `[276, 295]` 看不出渐变**：

- `vidToCoverScaleRatio.txt` 的 x/y 接近 1.0（配准没检测到缩放）
- 或者 `TransFrameNum` 太大，`ratio` 太小，肉眼看不出

**如果 `[276, 295]` 跳跃感强**：

- `TransFrameNum` 太小（比如 5 帧），过渡太快
- 或者配准精度不够导致 FOV 计算错误

### 3. 色彩是否正常

**对比 `Lp.mp4` 和 `LpOpt.mp4` 的色彩**：

- 如果 `LpOpt.mp4` **整体偏黄/偏红**——色彩范围不一致
- 如果 `LpOpt.mp4` **亮度明显偏低**——full-range 被按 limited-range 解码

**已修复**：`YUVImageReader` 现在用 `COLOR_BGR2YUV_I420` / `COLOR_YUV2BGR_I420`，与 FFmpeg limited-range 一致，色差应该明显减小。

**验证**：

```powershell
ffprobe -v error -select_streams v:0 `
  -show_entries stream=color_primaries,color_trc,colorspace,color_range `
  -of default=noprint_wrappers=1 "data/LivePhotoSmoothPlay/LpOpt.mp4"
```

**期望**：

```text
color_primaries=bt709
color_trc=bt709
colorspace=bt709
color_range=tv
```

### 4. 编码质量是否达标

```powershell
ffprobe -v error -select_streams v:0 `
  -show_entries stream=codec_name,width,height,r_frame_rate,bit_rate `
  -of default=noprint_wrappers=1 "data/LivePhotoSmoothPlay/LpOpt.mp4"
```

**期望**：

```text
codec_name=h264
width=720
height=1584
r_frame_rate=90/1
bit_rate=~4000000        ← 4 Mbps（CRF 不生效时）
```

**如果 `bit_rate` 只有 400 kbps**——说明用了旧的 `InsertMultiYuvs`（已删除），主流程走 `ReplaceMultiYuvs` 用 libx264 CRF20，`bit_rate` 会体现为 CRF 的实际码率。

---

## 调试方法

### 崩溃定位

**Release 下调试崩溃**：

1. VS 顶部构建配置选 `x64-release`
2. `调试 → 开始调试`（F5）
3. 崩溃弹窗点 **"中断"**
4. `Ctrl+Alt+C` 打开调用堆栈 → 右键"显示外部代码"
5. 找最上面属于 `CameraUtils.exe` 的帧，双击跳到源码

**常见崩溃位置**：

| 栈顶 | 原因 | 修复 |
| :--- | :--- | :--- |
| `cv::split` / `cv::merge` → `AutoBuffer::allocate` | 输入 Mat 尺寸/类型不对 | 检查 `YUV_W` / `YUV_H` 与实际是否一致 |
| `avcodec_send_frame` 返回负 | 编码器参数不匹配 | 检查 `pix_fmt` / 宽高 |
| `fread` 返回 0 | YUV 文件大小不足 | 检查 `DecodeMp4ToYuvFrames` 是否完整跑完 |

### 配准精度差

**症状**：`04_overlay_result.jpg` 看到"两套边缘"，`内点数 < 50`。

| 原因 | 修复 |
| :--- | :--- |
| `LAST_VID_FRAME_IDX` 填错 | 重新人眼找，改成正确值后**重编译** |
| `COVER_FRAME_IDX` 不是 `LAST_VID_FRAME_IDX + 21` | 检查 `FFmpegDefine.h` |
| ORB 5000 个点不够 | 改 `ImageRegistrator.cpp` 的 `cv::ORB::create(10000)` |
| RANSAC 3.0 太严格 | 改 `estimateTransform` 的 `findHomography(..., 5.0, ...)` |

### FOV 渐变不平滑

**症状**：`LpOpt.mp4` 的 `[276, 295]` 区间能看到"跳跃"。

| 原因 | 修复 |
| :--- | :--- |
| `TransFrameNum` 太小 | 改 `FFmpegDefine.h` 的 `TransFrameNum = 30`，并同步 `COVER_FRAME_IDX = LAST_VID_FRAME_IDX + TransFrameNum + 1` |
| `ratio` 计算错误 | 检查 `GenDiffFovYuvsFromCoverFrame` |
| `cropN.yuv` 生成时 resize 异常 | 检查 `CenterCropGenYuvs` 是否打出 `resized_ok=yes` |

### 色彩偏移

**症状**：`LpOpt.mp4` 比 `Lp.mp4` 明显偏黄/偏红。

**已修复**：`YUVImageReader` 现在用 I420 limited-range。如果仍有色差，检查：

- FFmpeg 编码是否显式设 `color_range`
- 播放器是否按 `color_primaries` 正确解码

---

## 与 `UpsampleVideo`（OpticalFlow 插帧）的区别

本测试处理的是 **LivePhoto 过渡帧**，与 `OpticalFlow` 模块的 `UpsampleVideo`（视频插帧）是**两个独立特性**：

| 维度 | `LivePhotoSmoothPlay` | `UpsampleVideo`（OpticalFlow） |
| :--- | :--- | :--- |
| **目标** | 把"突兀切到封面帧"改成"FOV 渐变" | 把 30fps 视频插帧到 60/90/120fps |
| **输入** | `Lp.mp4`（已含 alpha 混合帧） | 任意视频 |
| **输出** | `LpOpt.mp4`（替换过渡帧） | `1_60fps.mp4`（帧率翻倍） |
| **算法** | 配准 + 中心裁剪 + resize | 双向光流 + warp + 融合 |
| **关键约束** | 分辨率必须 720×1584 | 无分辨率约束 |
| **配准依赖** | 需要配准封面帧 ↔ 视频帧 | 不需要 |
| **测试入口** | `RunLivePhotoSmoothPlayTest()` | `Upsample30To60()` |

---

## 相关文档

- **模块接口**：[`../../inc/LivePhotoSmoothPlay/README.md`](../../inc/LivePhotoSmoothPlay/README.md)
- **实现总览**：[`../../src/LivePhotoSmoothPlay/README.md`](../../src/LivePhotoSmoothPlay/README.md)
- **实现细节**：[`../../src/LivePhotoSmoothPlay/IMPLEMENTATION_DETAILS.md`](../../src/LivePhotoSmoothPlay/IMPLEMENTATION_DETAILS.md)
- **配置说明**：[`../../configs/README.md`](../../configs/README.md)