# InterpolationConfig.json 配置说明

## 概述

`InterpolationConfig.json` 用于配置视频插帧模块（`UpsampleVideo` / `Upsample30To60`）的全部运行参数，
包括：输出倍率、编码质量、光流算法、遮挡处理、时域平滑等。

**关键特性**：
- 首次运行程序时，如果文件不存在会自动生成一份默认配置
- **修改本文件不需要重新编译**，直接重新运行 `CameraUtils.exe` 即可生效
- 所有字段都有默认值，缺失字段自动使用默认值

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

### 画质优先（慢一点，ghost 更少）

```json
{
    "preset": "medium",
    "crf": 18,
    "disPreset": 2,
    "maxCoarseLevels": 2,
    "fbThreshold": 0.5
}
```

### 速度优先（最快，画质妥协）

```json
{
    "preset": "ultrafast",
    "disPreset": 0,
    "maxCoarseLevels": 0,
    "enableTemporalSmooth": false,
    "enableStaticProtect": false
}
```

---

## 参数分组说明

### 一、插帧倍率

#### `factor`（整数，默认 `2`）

输出倍率。`factor = N` 表示每对相邻帧插入 `N-1` 张中间帧，输出帧率 = 源帧率 × N。

| 值 | 含义 | 使用场景 |
| :---: | :--- | :--- |
| `2` | 2× 插帧（30→60fps） | 常规视频流畅化 |
| `3` | 3× 插帧（30→90fps） | VR/高刷屏 |
| `4` | 4× 插帧（30→120fps） | 慢动作素材、电竞屏 |
| `1` | 无插帧（禁止，代码会报错退出） | — |

**注意**：`factor` 越大，每对帧要生成的中间帧越多，融合阶段耗时线性增长。
光流只计算一次（`ComputeFlows`），中间帧复用（`BlendFromFlows`），所以不是 `factor` 倍慢。

---

### 二、编码参数（输出 MP4）

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

**推荐**：先用 CRF 模式（`crf = 18~23`），追求可预测的文件大小时再切 ABR（`crf = -1` + 合适的 `bitrate`）。

---

### 三、光流后端

#### `flowBackend`（整数，默认 `1`）

光流算法类型。

| 值 | 算法 | 特点 |
| :---: | :--- | :--- |
| `0` | Farneback | 稠密光流，平滑但对大位移不准，**较慢** |
| **`1`** | **DIS**（Dense Inverse Search） | **默认，速度快，精度较好** |

**建议**：始终用 `1`。`0` 仅用于对比实验。

#### `disPreset`（整数，默认 `0`）

DIS 算法的速度/精度档位（仅 `flowBackend == 1` 时生效）。

| 值 | DIS 预设 | 相对速度 | 光流精度 | 推荐场景 |
| :---: | :--- | :---: | :---: | :--- |
| **`0`** | **ULTRAFAST** | **最快** | 一般 | **默认，日常使用** |
| `1` | FAST | 中 | 中高 | 平衡 |
| `2` | MEDIUM | 慢 | 高 | 画质优先，**ghost 明显时切换到这一档** |

#### `maxCoarseLevels`（整数，默认 `1`）

大位移处理的粗金字塔层数。在低分辨率计算 Farneback 粗光流，上采样作为 DIS 的初值，让 DIS 从更好的初值开始精修。

| 值 | 含义 | 每对帧额外耗时 | 适用场景 |
| :---: | :--- | :---: | :--- |
| `0` | 禁用大位移处理 | 0 ms | 慢速/静态视频，追求速度 |
| **`1`** | **1 层粗算（1/2 分辨率）** | **+15 ms** | **默认，普通运动** |
| `2` | 2 层粗算（1/4 分辨率） | +25 ms | 快速运动、摄像机摇 |
| `3` | 3 层粗算（1/8 分辨率） | +35 ms | 极端大位移（罕见） |

**注意**：粗算结果噪声大时，可能让 DIS 收敛到错误方向，反而产生伪影。**如果 `1` 效果不好，先试 `0`**。

---

### 四、前后向一致性（遮挡检测）

#### `fbThreshold`（浮点数，默认 `1.0`，单位：像素）

前后向一致性阈值。对每个像素计算 `err = |F_fwd(p) + F_bwd(p + F_fwd(p))|`：

- `err < fbThreshold` → 判定为"一致"（`confidence = 1`），走 `candidateWarped`
- `err >= fbThreshold` → 判定为"遮挡或错采"（`confidence = 0`），走 `candidateOccluded`

| 值 | 效果 |
| :---: | :--- |
| `0.3` | **最严格**，最多像素被判为遮挡 → **ghost 少但画面偏糊** |
| `0.5` | 严格 |
| **`1.0`** | **默认，平衡** |
| `2.0` | 宽松，遮挡判定少 → 边缘清晰但 ghost 增多 |
| `5.0` | 极宽松，几乎全部判为"一致" |

**调节建议**：
- **ghost 明显** → 降到 `0.5` 或 `0.3`
- 画面过于模糊 → 升到 `1.5` 或 `2.0`

---

### 五、静态区保护

#### `enableStaticProtect`（布尔，默认 `true`）

是否启用静态区保护。开启后，`|F| < staticMagThreshold` 的区域直接用 `(1-t)*prev + t*curr` 线性混合，不经过光流 warp。

| 值 | 效果 |
| :---: | :--- |
| **`true`** | **静止画面完全无抖动**（推荐） |
| `false` | 静态区也走光流，可能出现细微抖动 |

#### `staticMagThreshold`（浮点数，默认 `1.5`，单位：像素）

判定为"静态"的光流幅值阈值。`|F| < staticMagThreshold` → 静态区。

| 值 | 效果 |
| :---: | :--- |
| `0.5` | 只有真正静止的区域算静态（严格） |
| **`1.5`** | **默认，覆盖轻微抖动** |
| `3.0` | 慢速移动的物体也算静态（宽松） |

---

### 六、遮挡区软化（权重模糊）

#### `confidenceBlurSigma`（浮点数，默认 `3.0`，单位：像素）

`confidence` 掩膜的高斯模糊 sigma。`confidence` 本身是 0/1 二值，模糊后变成 0~1 连续值，遮挡边界更平滑。

| 值 | 效果 |
| :---: | :--- |
| `0.0` | 不模糊，遮挡边界硬切换 → **可能看到明显过渡线** |
| `1.5` | 轻微模糊，边界清晰 |
| **`3.0`** | **默认，边界较平滑** |
| `5.0` | 强模糊，遮挡区显著变"糊" |

#### `occlusionBlurSigma`（浮点数，默认 `2.0`，单位：像素）

`occWeight`（基于 `|F|` 的遮挡权重）的高斯模糊 sigma。

| 值 | 效果 |
| :---: | :--- |
| `0.0` | 不模糊 |
| **`2.0`** | **默认** |
| `4.0` | 更强的过渡 |

---

### 七、时域平滑（光流 EMA）

#### `enableTemporalSmooth`（布尔，默认 `true`）

是否启用光流时域平滑。开启后，缓存上一对帧的 `flowFwd`，与小位移区域做 EMA。

| 值 | 效果 |
| :---: | :--- |
| **`true`** | **静止画面更平滑**，减少帧间闪烁 |
| `false` | 每帧独立，可能出现轻微抖动 |

#### `temporalAlpha`（浮点数，默认 `0.85`，范围 0~1）

EMA 权重：`smoothFlow = alpha * newFlow + (1-alpha) * prevFlow`。

| 值 | 效果 |
| :---: | :--- |
| `1.0` | 完全不用缓存（等于关闭时域平滑） |
| `0.95` | 主要信任新光流，几乎不滞后 |
| **`0.85`** | **默认，轻微平滑** |
| `0.7` | 更信任旧光流，静态画面更稳但**可能跟不上突变** |
| `0.5` | 新旧各半，滞后明显 |

**注意**：`temporalAlpha` 越小，静态越稳，但**突变场景（场景切换、物体突然出现）会有 1~2 帧的"拖尾"**。建议 `0.85 ~ 0.95`。

#### `temporalMagThreshold`（浮点数，默认 `5.0`，单位：像素）

只对 `|F| < temporalMagThreshold` 的区域做时域平滑。大位移区域完全用新光流，避免滞后。

| 值 | 效果 |
| :---: | :--- |
| `3.0` | 只对极小位移做平滑（保守） |
| **`5.0`** | **默认** |
| `10.0` | 中等运动也平滑（可能滞后） |

---

## 完整参数速查表

| 参数 | 类型 | 默认值 | 取值范围 | 分组 |
| :--- | :---: | :---: | :--- | :--- |
| `factor` | int | `2` | ≥ 2 | 倍率 |
| `bitrate` | int | `16000000` | 任意正数 | 编码 |
| `preset` | string | `"veryfast"` | x264 preset | 编码 |
| `crf` | int | `20` | -1 或 0~51 | 编码 |
| `flowBackend` | int | `1` | 0 或 1 | 光流 |
| `disPreset` | int | `0` | 0~2 | 光流 |
| `maxCoarseLevels` | int | `1` | 0~3 | 光流 |
| `fbThreshold` | double | `1.0` | 0.1~5.0 | 一致性 |
| `enableStaticProtect` | bool | `true` | true/false | 静态 |
| `staticMagThreshold` | double | `1.5` | 0.5~5.0 | 静态 |
| `confidenceBlurSigma` | double | `3.0` | 0.0~10.0 | 软化 |
| `occlusionBlurSigma` | double | `2.0` | 0.0~10.0 | 软化 |
| `enableTemporalSmooth` | bool | `true` | true/false | 时域 |
| `temporalAlpha` | double | `0.85` | 0.5~1.0 | 时域 |
| `temporalMagThreshold` | double | `5.0` | 1.0~20.0 | 时域 |

---

## 典型调参场景

### 场景 1：ghost（重影）明显

按以下顺序**逐个**尝试（每改一项重新运行对比）：

```json
{
    "fbThreshold": 0.5,
    "disPreset": 2,
    "maxCoarseLevels": 2
}
```

代价：光流计算慢 2~3 倍。

### 场景 2：静止画面有抖动

```json
{
    "enableStaticProtect": true,
    "staticMagThreshold": 2.5,
    "enableTemporalSmooth": true,
    "temporalAlpha": 0.75
}
```

### 场景 3：快速运动场景跟不上

```json
{
    "maxCoarseLevels": 2,
    "temporalMagThreshold": 3.0,
    "temporalAlpha": 0.95
}
```

### 场景 4：追求最快速度（预览用）

```json
{
    "preset": "ultrafast",
    "crf": 28,
    "disPreset": 0,
    "maxCoarseLevels": 0,
    "enableStaticProtect": false,
    "enableTemporalSmooth": false,
    "confidenceBlurSigma": 0.0,
    "occlusionBlurSigma": 0.0
}
```

预期：264 对帧从 ~25 秒降到 ~12 秒。

### 场景 5：发布级画质

```json
{
    "preset": "medium",
    "crf": 18,
    "disPreset": 2,
    "maxCoarseLevels": 2,
    "fbThreshold": 0.5,
    "confidenceBlurSigma": 1.5
}
```

预期：264 对帧耗时 ~40 秒，画质接近商业插帧。

---

## 常见问题

### Q1：修改 JSON 后重新运行，日志显示的还是旧值

**原因**：文件编码带 UTF-8 BOM，`nlohmann/json` 解析失败，回退到默认值。

**验证**：看日志第一行：

- `[InterpolationConfig] Loaded: ... (factor=3, ...)` → 配置加载成功
- `[InterpolationConfig] ... not found, creating default` → JSON 不存在或解析失败，被覆盖成默认值

**修复**：用 VS Code 右下角编码选择器，改成 **"UTF-8"**（不是 "UTF-8 with BOM"）。
或 PowerShell 7 中 `Set-Content -Encoding utf8`（无 BOM）。

### Q2：修改 JSON 后是否需要重新编译？

**不需要**。`InterpolationConfig::LoadFromFile` 在程序启动时读取 JSON。
只有修改 **`.h` / `.cpp` 源码** 时才需要 `cmake --build`。

### Q3：JSON 里少写字段会怎样？

缺失字段自动使用默认值，**不会报错**。例如：

```json
{
    "factor": 3
}
```

只有 `factor = 3` 生效，其他字段全部用默认值。**首次运行**时程序会生成一份完整默认配置，之后你可以按需删减。

### Q4：JSON 里多了未知字段会怎样？

**会被忽略**，不报错。例如：

```json
{
    "factor": 2,
    "unknownField": "abc"
}
```

### Q5：改了 `factor` 后旧输出文件会被覆盖吗？

**不会**。输出文件按帧率命名：

- `factor = 2` → `1_60fps.mp4`
- `factor = 3` → `1_90fps.mp4`
- `factor = 4` → `1_120fps.mp4`

目录里会同时存在多个文件，手动删不需要的即可。

### Q6：不想改 JSON，能否用环境变量覆盖？

**当前不支持**。所有参数都从 JSON 读，如需命令行覆盖，可在 `Upsample30To60` 里手动改 `cfg` 字段（代码里改，重编译）。

---

## 相关文件

- `inc/OpticalFlow/InterpolationConfig.h` — 配置结构体定义（`NLOHMANN_DEFINE_TYPE_INTRUSIVE`）
- `src/OpticalFlow/FrameInterpolator.cpp` — 光流计算 + 融合核心实现
- `tests/OpticalFlowTest.h` — `UpsampleVideo` / `Upsample30To60` 入口
- `configs/JsonConfig.json` — 全局 JSON 工具配置（与本文件独立，勿混淆）