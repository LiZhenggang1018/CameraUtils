# InterpolationConfig.json 配置说明

## 概述

`InterpolationConfig.json` 用于配置视频插帧模块（`UpsampleVideo` / `Upsample30To60`）的全部运行参数，
包括：输出倍率、编码质量、光流算法、性能档位、遮挡处理、时域平滑等。

**关键特性**：
- 首次运行程序时，如果文件不存在会自动生成一份默认配置
- **修改本文件不需要重新编译**，直接重新运行 `CameraUtils.exe` 即可生效
- 所有字段都有默认值，缺失字段自动使用默认值
- 支持 `performanceLevel` 一键切换性能档位，也支持细粒度手动调参

**文件位置**：`<项目根>/configs/InterpolationConfig.json`

---

## 快速上手

### 只改输出倍率（30fps → 60/90/120fps）

```json
{
    "factor": 3
}
```

| `factor` | 输出帧率（30fps 源） | 输出文件名 | 每对帧插入中间帧数 |
| :---: | :---: | :--- | :---: |
| 2 | 60 fps | `1_60fps.mp4` | 1 张（t=0.5） |
| **3** | **90 fps** | **`1_90fps.mp4`** | **2 张（t=1/3, 2/3）** |
| 4 | 120 fps | `1_120fps.mp4` | 3 张（t=1/4, 2/4, 3/4） |

### 一键切换性能档位

只改一个数字 `performanceLevel`：

```json
{
    "performanceLevel": 1
}
```

| 档位 | 总耗时（264 帧 1080p） | 画质 |
| :---: | :---: | :--- |
| 0 | ~10 秒 | 中 |
| **1** | **~20 秒** | **中高** |
| 2 | ~30~45 秒 | 高（默认） |
| 3 | ~70 秒 | 很高 |
| 4 | ~2~3 分钟 | 最高（TV-L1） |

### 画质优先

```json
{
    "performanceLevel": 3,
    "preset": "medium",
    "crf": 18
}
```

### 速度优先

```json
{
    "performanceLevel": 0,
    "preset": "ultrafast",
    "crf": 28
}
```

---

## 参数分组说明

### 一、性能档位

#### `performanceLevel`（整数，默认 `2`）

**一键控制性能与画质的平衡**。设置 `-1` 表示"不启用预设"，完全按细粒度字段手动组合。

| 值 | 档位 | 总耗时（264 帧） | 使用的参数组合 |
| :---: | :--- | :---: | :--- |
| `0` | 极速 | ~10 秒 | DIS ULTRAFAST + 1/4 分辨率 + 关闭所有后处理 |
| `1` | 快 | ~20 秒 | DIS ULTRAFAST + 1/2 分辨率 + 关闭后处理 |
| **`2`** | **平衡（默认）** | **~30~45 秒** | DIS FAST + 全分辨率 + 全部开启 |
| `3` | 画质 | ~70 秒 | DIS MEDIUM + 大位移 + 全部开启 |
| `4` | 极致 | ~2~3 分钟 | TV-L1 光流 + 1/2 分辨率 + 全部开启 |
| `-1` | **自定义** | 取决于细粒度字段 | **不覆盖任何字段**，完全使用 JSON 中手动设置的值 |

**注意**：
- 当 `performanceLevel >= 0` 时，`ApplyPerformanceLevel()` 会**覆盖** JSON 里对应的细粒度字段（`flowBackend`、`flowDownscale`、`enableXxx` 等）
- 当 `performanceLevel = -1` 时，所有细粒度字段**原样生效**，你可以自由组合

### 各档位的完整参数对照

| 参数 | Level 0 | Level 1 | Level 2 | Level 3 | Level 4 |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `flowBackend` | 1(DIS) | 1(DIS) | 1(DIS) | 1(DIS) | **2(TV-L1)** |
| `disPreset` | 0(ULTRAFAST) | 0(ULTRAFAST) | 1(FAST) | 2(MEDIUM) | — |
| `maxCoarseLevels` | 0 | 0 | 1 | 2 | — |
| `flowDownscale` | 0.25 | 0.5 | 1.0 | 1.0 | 0.5 |
| `enableBilateralPreprocess` | false | false | true | true | true |
| `enableFlowPostProcess` | false | false | true | true | true |
| `enableLaplacianBlend` | false | true | true | true | true |
| `laplacianLevels` | — | 3 | 4 | 4 | 4 |
| `enableTemporalSmooth` | false | true | true | true | true |

---

### 二、插帧倍率

#### `factor`（整数，默认 `2`）

输出倍率。`factor = N` 表示每对相邻帧插入 `N-1` 张中间帧，输出帧率 = 源帧率 × N。

| 值 | 含义 | 使用场景 |
| :---: | :--- | :--- |
| `2` | 2× 插帧（30→60fps） | 常规视频流畅化 |
| `3` | 3× 插帧（30→90fps） | VR/高刷屏 |
| `4` | 4× 插帧（30→120fps） | 慢动作素材、电竞屏 |

**注意**：`factor` 越大，每对帧要生成的中间帧越多，融合阶段耗时线性增长。
光流只计算一次（`ComputeFlows`），中间帧复用（`BlendFromFlows`），所以不是 `factor` 倍慢。

---

### 三、编码参数（输出 MP4）

#### `bitrate`（整数，默认 `16000000`，单位 bps）

目标平均码率（ABR 模式）。**仅在 `crf < 0` 时生效**。

| 分辨率/帧率 | 推荐值 |
| :--- | :--- |
| 720p @ 60fps | 4,000,000 ~ 6,000,000 |
| **1080p @ 60fps** | **12,000,000 ~ 20,000,000** |
| 1080p @ 90/120fps | 20,000,000 ~ 30,000,000 |
| 4K @ 60fps | 50,000,000 ~ 80,000,000 |

#### `preset`（字符串，默认 `"veryfast"`）

libx264 编码速度预设。从快到慢、从低质量到高质量：

| 值 | 相对速度 | 相对压缩率 | 推荐场景 |
| :--- | :---: | :---: | :--- |
| `"ultrafast"` | 最快 | 最低 | 调试、快速预览 |
| **`"veryfast"`** | **快** | **中** | **默认，日常使用** |
| `"fast"` | 中 | 中高 | 平衡 |
| `"medium"` | 慢 | 高 | 发布级画质 |
| `"slow"` | 更慢 | 更高 | 追求极致压缩 |
| `"veryslow"` | 最慢 | 最高 | 存档 |

#### `crf`（整数，默认 `20`）

恒定质量因子（CRF 模式）。`crf >= 0` 时**忽略 `bitrate`**，用 CRF 模式。

| CRF 值 | 画质 | 相对码率 |
| :---: | :--- | :--- |
| `0` | 无损 | 极大 |
| `18` | 视觉无损 | 大 |
| **`20`** | **高质量** | **中**（默认） |
| `23` | 良好 | 中低 |
| `28` | 一般 | 小 |
| `35` | 差 | 极小 |
| `-1` | 不用 CRF（改用 `bitrate`） | — |

---

### 四、光流后端

#### `flowBackend`（整数，默认 `1`）

光流算法类型。

| 值 | 算法 | 特点 |
| :---: | :--- | :--- |
| `0` | Farneback | 稠密光流，平滑但对大位移不准，**较慢** |
| **`1`** | **DIS**（Dense Inverse Search） | **默认，速度快，精度较好** |
| `2` | TV-L1（Dual TV-L1） | 边缘保持最优，**极慢**（比 DIS 慢 10~15 倍） |

**建议**：日常用 `1`（DIS）。`2`（TV-L1）只在"文本/UI 内容、边缘清晰度要求极高"时用。

#### `disPreset`（整数，默认 `0`）

DIS 算法的速度/精度档位（仅 `flowBackend == 1` 时生效）。

| 值 | DIS 预设 | 相对速度 | 光流精度 | 推荐场景 |
| :---: | :--- | :---: | :---: | :--- |
| **`0`** | **ULTRAFAST** | **最快** | 一般 | 速度优先、预览 |
| `1` | FAST | 中 | 中高 | 平衡（默认） |
| `2` | MEDIUM | 慢 | 高 | 画质优先 |

#### `maxCoarseLevels`（整数，默认 `1`）

DIS 的大位移处理：在低分辨率算 Farneback 粗光流，上采样作为 DIS 的初值。

| 值 | 含义 | 每对帧额外耗时 | 适用场景 |
| :---: | :--- | :---: | :--- |
| `0` | 禁用 | 0 ms | 慢速/静态视频，追求速度 |
| **`1`** | **1 层（1/2 分辨率）** | **+15 ms** | **默认，普通运动** |
| `2` | 2 层（1/4 分辨率） | +25 ms | 快速运动、摄像机摇 |

**注意**：
- 只在 `flowDownscale = 1.0` 且 `flowBackend == 1`（DIS）时生效
- `flowDownscale < 1.0` 时自动跳过（避免重复降采样）

---

### 五、性能细粒度开关

#### `flowDownscale`（浮点数，默认 `1.0`）

**光流计算分辨率系数**。**这是最重要的性能参数**——光流是最大瓶颈。

| 值 | 光流计算分辨率（1080p 输入） | 光流算力 | 每对帧省时 | 画质影响 |
| :---: | :--- | :---: | :---: | :--- |
| `1.0` | 1920×1088 | 基准 | 0 | 无 |
| **`0.5`** | **960×544** | **1/4** | **~-70 ms** | 微小 |
| `0.25` | 480×272 | **1/16** | **~-105 ms** | 中等（精细边缘模糊） |

**原理**：降采样后算光流 → 上采样回全分辨率 → 位移值 × (1/scale)。

#### `enableBilateralPreprocess`（布尔，默认 `true`）

光流计算前是否对灰度图做双边滤波。**减少纹理噪声**，让 DIS 光流更稳定。

| 值 | 每对帧耗时 | 画质影响 |
| :---: | :---: | :--- |
| `true` | +60 ms | 光流更平滑，减少局部跳变 |
| `false` | 0 | 光流可能有 1~2 像素噪声 |

#### `enableFlowPostProcess`（布尔，默认 `true`）

光流后处理：中值滤波 + 引导滤波（`ximgproc::guidedFilter`）。**让光流跟随图像边缘**。

| 值 | 每对帧耗时 | 画质影响 |
| :---: | :---: | :--- |
| `true` | **+200~400 ms**（最大瓶颈） | 光流在物体边界处更准确 |
| `false` | 0 | 光流在边缘处可能跨边界，ghost 略增 |

**注意**：这是**最慢的开关**。追求速度时**优先关闭它**——DIS 光流本身已经不错，guidedFilter 的收益有限。

#### `enableLaplacianBlend`（布尔，默认 `true`）

拉普拉斯金字塔融合。**大色块无 ghost + 细节清晰**——这是方案 3 的核心。

| 值 | 每对帧耗时 | 画质影响 |
| :---: | :---: | :--- |
| `true` | +30 ms | 大色块走 linear、细节走 warp，两个优点都拿到 |
| `false` | 0 | 退化为粗融合，ghost 和清晰度都差一截 |

#### `laplacianLevels`（整数，默认 `4`）

拉普拉斯金字塔层数。

| 值 | 大色块覆盖能力 | 每对帧耗时 |
| :---: | :--- | :---: |
| `2` | 4 像素尺度（小色块） | +15 ms |
| `3` | 8 像素尺度（中色块） | +22 ms |
| **`4`** | **16 像素尺度（大色块）** | **+30 ms** |

**建议**：`4` 是画质最优值。只有极限速度优化时才降到 3 或 2。

---

### 六、前后向一致性（遮挡检测）

#### `fbThreshold`（浮点数，默认 `1.0`，单位：像素）

前后向一致性阈值。`err = |F_fwd(p) + F_bwd(p + F_fwd(p))|`：

- `err < fbThreshold` → 判定为"一致"，走 `candidateWarped`
- `err >= fbThreshold` → 判定为"遮挡或错采"，走 `candidateOccluded`

| 值 | 效果 |
| :---: | :--- |
| `0.3` | 最严格，最多像素被判为遮挡 → ghost 少但画面偏糊 |
| `0.5` | 严格 |
| **`1.0`** | **默认，平衡** |
| `2.0` | 宽松，边缘清晰但 ghost 增多 |
| `5.0` | 极宽松，几乎全部判为"一致" |

---

### 七、静态区保护

#### `enableStaticProtect`（布尔，默认 `true`）

是否启用静态区保护。开启后，`|F| < staticMagThreshold` 的区域直接用 `(1-t)*prev + t*curr` 线性混合。

| 值 | 效果 |
| :---: | :--- |
| **`true`** | **静止画面完全无抖动**（推荐） |
| `false` | 静态区也走光流，可能出现细微抖动 |

#### `staticMagThreshold`（浮点数，默认 `1.5`，单位：像素）

判定为"静态"的光流幅值阈值。

| 值 | 效果 |
| :---: | :--- |
| `0.5` | 只有真正静止的区域算静态（严格） |
| `1.5` | 默认 |
| **`3.0`** | **文本区域（轻微抖动）也算静态**（推荐，避免文本被误判为遮挡） |

---

### 八、遮挡区软化（权重模糊）

#### `confidenceBlurSigma`（浮点数，默认 `3.0`，单位：像素）

`confidence` 掩膜的高斯模糊 sigma。

| 值 | 效果 |
| :---: | :--- |
| `0.0` | 不模糊，遮挡边界硬切换 |
| **`1.0`** | **轻微模糊（推荐）** |
| `3.0` | 默认，边界较平滑 |
| `5.0` | 强模糊，遮挡区显著变"糊" |

#### `occlusionBlurSigma`（浮点数，默认 `2.0`，单位：像素）

`occWeight`（基于 `|F|` 的遮挡权重）的高斯模糊 sigma。

| 值 | 效果 |
| :---: | :--- |
| `0.0` | 不模糊 |
| **`1.0`** | **轻微模糊（推荐）** |
| `2.0` | 默认 |

---

### 九、时域平滑（光流 EMA）

#### `enableTemporalSmooth`（布尔，默认 `true`）

是否启用光流时域平滑。缓存上一对帧的 `flowFwd`，与小位移区域做 EMA。

| 值 | 效果 |
| :---: | :--- |
| **`true`** | **静止画面更平滑**，减少帧间闪烁 |
| `false` | 每帧独立，可能出现轻微抖动 |

#### `temporalAlpha`（浮点数，默认 `0.85`，范围 0~1）

EMA 权重：`smoothFlow = alpha * newFlow + (1-alpha) * prevFlow`。

| 值 | 效果 |
| :---: | :--- |
| `1.0` | 完全不用缓存（等于关闭时域平滑） |
| **`0.85`** | **默认，轻微平滑** |
| `0.7` | 更信任旧光流，静态更稳但可能跟不上突变 |
| `0.5` | 新旧各半，滞后明显 |

#### `temporalMagThreshold`（浮点数，默认 `5.0`，单位：像素）

只对 `|F| < temporalMagThreshold` 的区域做时域平滑。

| 值 | 效果 |
| :---: | :--- |
| `3.0` | 只对极小位移做平滑（保守） |
| **`5.0`** | **默认** |
| `10.0` | 中等运动也平滑（可能滞后） |

---

## 完整参数速查表

| 参数 | 类型 | 默认值 | 取值范围 | 分组 |
| :--- | :---: | :---: | :--- | :--- |
| `performanceLevel` | int | `2` | -1~4 | 档位 |
| `factor` | int | `2` | ≥ 2 | 倍率 |
| `bitrate` | int | `16000000` | 任意正数 | 编码 |
| `preset` | string | `"veryfast"` | x264 preset | 编码 |
| `crf` | int | `20` | -1 或 0~51 | 编码 |
| `flowBackend` | int | `1` | 0/1/2 | 光流 |
| `disPreset` | int | `0` | 0~2 | 光流 |
| `maxCoarseLevels` | int | `1` | 0~2 | 光流 |
| `flowDownscale` | double | `1.0` | 0.25/0.5/1.0 | 性能 |
| `enableBilateralPreprocess` | bool | `true` | true/false | 性能 |
| `enableFlowPostProcess` | bool | `true` | true/false | 性能 |
| `enableLaplacianBlend` | bool | `true` | true/false | 性能 |
| `laplacianLevels` | int | `4` | 2~4 | 性能 |
| `fbThreshold` | double | `1.0` | 0.3~5.0 | 一致性 |
| `enableStaticProtect` | bool | `true` | true/false | 静态 |
| `staticMagThreshold` | double | `1.5` | 0.5~5.0 | 静态 |
| `confidenceBlurSigma` | double | `3.0` | 0.0~5.0 | 软化 |
| `occlusionBlurSigma` | double | `2.0` | 0.0~4.0 | 软化 |
| `enableTemporalSmooth` | bool | `true` | true/false | 时域 |
| `temporalAlpha` | double | `0.85` | 0.5~1.0 | 时域 |
| `temporalMagThreshold` | double | `5.0` | 1.0~20.0 | 时域 |

---

## 典型调参场景

### 场景 1：ghost（重影）明显

```json
{
    "performanceLevel": -1,
    "flowBackend": 1,
    "disPreset": 2,
    "maxCoarseLevels": 2,
    "flowDownscale": 1.0,
    "fbThreshold": 0.5,
    "enableFlowPostProcess": true,
    "enableLaplacianBlend": true
}
```

### 场景 2：文本/边缘模糊（幽灵化）

```json
{
    "performanceLevel": -1,
    "fbThreshold": 1.5,
    "staticMagThreshold": 3.0,
    "enableFlowPostProcess": false,
    "enableLaplacianBlend": true
}
```

`guidedFilter` 会把文本边缘平滑掉，`enableFlowPostProcess: false` 保留文本细节。

### 场景 3：静止画面有抖动

```json
{
    "enableStaticProtect": true,
    "staticMagThreshold": 2.5,
    "enableTemporalSmooth": true,
    "temporalAlpha": 0.75
}
```

### 场景 4：追求最快速度（预览用）

```json
{
    "performanceLevel": 0
}
```

或手动：

```json
{
    "performanceLevel": -1,
    "flowBackend": 1,
    "disPreset": 0,
    "flowDownscale": 0.25,
    "enableBilateralPreprocess": false,
    "enableFlowPostProcess": false,
    "enableLaplacianBlend": false,
    "enableTemporalSmooth": false,
    "preset": "ultrafast",
    "crf": 28
}
```

预期 ~8~10 秒（264 对帧）。

### 场景 5：发布级画质

```json
{
    "performanceLevel": -1,
    "flowBackend": 1,
    "disPreset": 2,
    "maxCoarseLevels": 2,
    "flowDownscale": 1.0,
    "fbThreshold": 0.5,
    "enableFlowPostProcess": true,
    "enableLaplacianBlend": true,
    "laplacianLevels": 4,
    "preset": "medium",
    "crf": 18
}
```

预期 ~70 秒。

### 场景 6：文本/UI 内容（边缘清晰度最重要）

```json
{
    "performanceLevel": 4
}
```

或手动：

```json
{
    "performanceLevel": -1,
    "flowBackend": 2,
    "flowDownscale": 0.5,
    "fbThreshold": 1.0,
    "staticMagThreshold": 3.0,
    "enableFlowPostProcess": true,
    "enableLaplacianBlend": true
}
```

---

## 常见问题

### Q1：修改 JSON 后重新运行，日志显示的还是旧值

**原因**：文件编码带 UTF-8 BOM，`nlohmann/json` 解析失败，回退到默认值。

**验证**：看日志第一行：

- `[InterpolationConfig] Loaded: ... (performanceLevel=X, ...)` → 配置加载成功
- `[InterpolationConfig] ... not found, creating default` → JSON 不存在或解析失败

**修复**：用 VS Code 右下角编码选择器，改成 **"UTF-8"**（不是 "UTF-8 with BOM"）。

### Q2：修改 JSON 后是否需要重新编译？

**不需要**。`InterpolationConfig::LoadFromFile` 在程序启动时读取 JSON。
只有修改 **`.h` / `.cpp` 源码** 时才需要 `cmake --build`。

### Q3：`performanceLevel` 和细粒度字段冲突时听谁的？

- `performanceLevel >= 0` → **`ApplyPerformanceLevel()` 覆盖细粒度字段**
- `performanceLevel = -1` → **细粒度字段原样生效**

想完全控制就用 `-1`；想省事就用 0~4。

### Q4：改了 `factor` 后旧输出文件会被覆盖吗？

**不会**。输出文件按帧率命名：`1_60fps.mp4` / `1_90fps.mp4` / `1_120fps.mp4`。

### Q5：哪个参数影响速度最大？

按影响从大到小：

1. **`flowDownscale`**：0.5 时省 ~70 ms，0.25 时省 ~105 ms
2. **`enableFlowPostProcess`**：关闭省 200~400 ms
3. **`disPreset`**：0 (ULTRAFAST) vs 2 (MEDIUM) 差 ~50 ms
4. **`flowBackend`**：TV-L1 比 DIS 慢 10~15 倍
5. **`enableBilateralPreprocess`**：关闭省 ~60 ms

### Q6：为什么 TV-L1 光流这么慢？

TV-L1 是**迭代变分方法**，每层要 warp 5 次 × 5 层 = 25 次迭代，每次都是全分辨率 `remap`。这是变分光流的通病。**只在文本/UI 这种边缘清晰度硬要求时用**。

### Q7：`bilateralFilter` 崩溃怎么办？

**已知修复**：`ComputeFlows` 里 `cv::Mat prevForFlow;`（空 Mat）而不是 `cv::Mat prevForFlow = prevGray;`（浅拷贝）。src 和 dst 不能共享内存，否则 `bilateralFilterInvoker_8u` 多线程读写冲突崩溃。

### Q8：`LaplacianBlend` 崩溃怎么办？

**已知修复**：`buildPyramid(src, gp, levels - 1)`，不是 `buildPyramid(src, gp, levels)`。前者构建 `levels` 层，后者构建 `levels+1` 层导致尺寸不匹配。若代码未修，临时改 `enableLaplacianBlend: false` 绕过。

---

## 性能-画质权衡总结

| 场景 | 推荐 performanceLevel | 预期耗时 | 画质 |
| :--- | :---: | :---: | :--- |
| 快速预览 | 0 | ~10 秒 | 中 |
| 日常批量处理 | **1** | **~20 秒** | 中高 |
| **平衡（默认）** | **2** | **~30~45 秒** | 高 |
| 屏幕录制、UI 演示 | 3 | ~70 秒 | 很高 |
| 特殊素材（文本硬要求） | 4 | ~2~3 分钟 | 最高 |
| **自定义组合** | **-1** | 视配置而定 | 完全可控 |

---

## 相关文件

- `inc/OpticalFlow/InterpolationConfig.h` — 配置结构体 + `ApplyPerformanceLevel`
- `src/OpticalFlow/FrameInterpolator.cpp` — 光流计算 + 融合核心实现
- `src/OpticalFlow/FfmpegVideoWriter.cpp` — MP4 编码写出
- `tests/OpticalFlowTest.h` — `UpsampleVideo` / `Upsample30To60` 入口
- `configs/JsonConfig.json` — 全局 JSON 工具配置（与本文件独立，勿混淆）