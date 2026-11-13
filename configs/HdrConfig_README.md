# HdrConfig.json 配置说明

## 概述

`configs/HdrConfig.json` 用于配置 HDR 融合模块（`hdr_merge::HdrMergeFromFiles`）的全部运行参数，包括 Mertens 曝光融合权重、黑边裁剪、Unsharp Mask 锐化、detailEnhance、**质量档位定义表**等。

**关键特性**：

- 首次运行程序时，如果文件不存在会自动生成一份默认配置
- **修改本文件不需要重新编译**，直接重新运行 `CameraUtils.exe` 即可生效
- 支持 `qualityLevel` 一键切换质量档位；**档位的具体数值也定义在 JSON 里**，可增删改
- 支持 `qualityLevel = -1` 时完全手动控制所有细粒度字段

**文件位置**：`<项目根>/configs/HdrConfig.json`

---

## 快速上手

### 一键切换质量档位

只改一个数字 `qualityLevel`：

```json
{
    "qualityLevel": 2
}
```

| 值 | 档位 | 对比度 | 饱和 | 锐化 | 适合场景 |
| :---: | :--- | :---: | :---: | :---: | :--- |
| 0 | 柔和（Soft） | 低 | 自然 | ❌ | 人像、室内柔和场景 |
| **1** | **平衡（Balanced，默认）** | **中** | **略高** | **中** | **通用** |
| 2 | 锐利（Punchy） | 高 | 高 | 强 | 风景、建筑、细节丰富 |
| -1 | 自定义 | 由细粒度字段决定 | 同左 | 同左 | 完全手动控制 |

### 切换到柔和模式

```json
{
    "qualityLevel": 0
}
```

### 切换到锐利模式

```json
{
    "qualityLevel": 2
}
```

### 完全手动控制

```json
{
    "qualityLevel": -1,
    "contrastWeight": 1.3,
    "saturationWeight": 1.15,
    "unsharpAmount": 0.8,
    "unsharpThreshold": 1.5
}
```

---

## 参数分组说明

### 一、质量档位选择

#### `qualityLevel`（整数，默认 `1`）

**一键控制 HDR 融合风格**。设置 `-1` 表示"不启用预设"，完全按细粒度字段手动组合。

**查找逻辑**：

- `qualityLevel >= 0` → 代码查 `qualityPresets[std::to_string(qualityLevel)]`，找到后**覆盖**对应的细粒度字段（`contrastWeight` / `saturationWeight` / `exposureWeight` / `enableUnsharpMask` / `unsharpAmount` / `unsharpRadius` / `unsharpThreshold` / `enableDetailEnhance`）
- `qualityLevel = -1` → **不覆盖**，所有细粒度字段原样生效
- `qualityLevel >= 0` 但 `qualityPresets` 里没有对应 key → **打印警告**，退回用细粒度字段（不崩）

---

### 二、档位定义表：`qualityPresets`

**从 v2 起**，档位的具体参数值不再硬编码在代码里，而是存在 JSON 的 `qualityPresets` 字段中。

#### 结构

```json
"qualityPresets": {
    "<档位编号>": {
        "contrastWeight":      1.2,
        "saturationWeight":    1.1,
        "exposureWeight":      1.0,
        "enableUnsharpMask":   true,
        "unsharpAmount":       0.6,
        "unsharpRadius":       1.2,
        "unsharpThreshold":    0.0,
        "enableDetailEnhance": false
    },
    ...
}
```

#### 规则

- **key 是字符串**（如 `"0"`、`"1"`、`"2"`），但内容是数字
- `qualityLevel` 是整数时，代码查 `qualityPresets[std::to_string(qualityLevel)]`
- 找不到对应 key → **打印警告，退回用细粒度字段**（不会崩）
- 每个 preset 包含 8 个字段（与细粒度字段中的 Mertens/锐化部分完全对应）

#### 默认三档

| key | 名称 | contrastWeight | saturationWeight | exposureWeight | enableUnsharpMask | unsharpAmount | unsharpRadius | unsharpThreshold |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `"0"` | 柔和 | 0.8 | 1.0 | 1.0 | false | 0.6 | 1.2 | 0.0 |
| `"1"` | 平衡 | 1.2 | 1.1 | 1.0 | true | 0.6 | 1.2 | 0.0 |
| `"2"` | 锐利 | 1.5 | 1.2 | 1.2 | true | 1.0 | 1.5 | 2.0 |

> `enableDetailEnhance` 在三档中都是 `false`（detailEnhance 会改变色彩风格，默认不用）。

#### 增删档位

| 操作 | 做法 |
| :--- | :--- |
| **新增档位** | 加一个 key，如 `"3": {...}`；然后 `qualityLevel = 3` |
| **删除档位** | 删掉对应的 key；`qualityLevel` 不能再指向它 |
| **修改档位** | 改 key 下的字段值 |

#### 示例：新增"极致锐"档

在 `qualityPresets` 里追加：

```json
"3": {
    "contrastWeight": 1.8,
    "saturationWeight": 1.25,
    "exposureWeight": 1.3,
    "enableUnsharpMask": true,
    "unsharpAmount": 1.3,
    "unsharpRadius": 1.8,
    "unsharpThreshold": 3.0,
    "enableDetailEnhance": false
}
```

然后设 `"qualityLevel": 3`，**不需要重编译**。

#### ⚠️ 注意

- 档位 key 和 `qualityLevel` 必须能对上（`"0"` ↔ `0`）
- 档位数值在 JSON 里改，**代码不校验**——改坏了（比如 `contrastWeight = 10`）会导致严重光晕
- **建议保留 `0`/`1`/`2` 三个"参考档位"不动**，新增档位用 `3`/`4`/…
- 如果 `qualityPresets` 为空 map（`{}`）→ 所有档位都找不到 → 全部退化到细粒度字段

---

### 三、细粒度字段（Mertens 权重）

> **以下三个字段仅在 `qualityLevel = -1`，或 `qualityLevel >= 0` 但 `qualityPresets` 里找不到对应 key 时生效。**

#### `contrastWeight`（double，默认 `1.2`）

对比度权重。提高后亮暗边界更清晰，但过高会产生光晕。

| 值 | 效果 |
| :---: | :--- |
| `0.8` | 平滑柔和，但可能"发灰" |
| `1.0` | Mertens 默认 |
| **`1.2`** | **推荐；亮暗边界清晰，无光晕** |
| `1.5` | 更锐，但**亮暗交界可能出现白边光晕** |
| `2.0+` | 光晕明显，不建议 |

#### `saturationWeight`（double，默认 `1.1`）

饱和度权重。提高后色彩更艳。

| 值 | 效果 |
| :---: | :--- |
| `0.8` | 色彩素淡 |
| `1.0` | Mertens 默认 |
| **`1.1`** | **推荐；色彩略艳** |
| `1.5+` | 过饱和，可能失真 |

#### `exposureWeight`（double，默认 `1.0`）

曝光权重。提高后更"均衡曝光"。

| 值 | 效果 |
| :---: | :--- |
| `0.8` | 倾向于"保留原曝光风格" |
| **`1.0`** | **推荐** |
| `1.2` | 更均匀亮度 |
| `1.5+` | 可能过曝 |

---

### 四、黑边裁剪

> **以下四个字段始终生效**，不受 `qualityLevel` 影响。

#### `autoDetectBorder`（bool，默认 `true`）

是否自动检测配准后的黑边。

| 值 | 行为 |
| :---: | :--- |
| **`true`** | **逐行/列统计黑像素比例，找到有效内容区域** |
| `false` | 每边固定裁 `cropRatio` |

#### `cropRatio`（double，默认 `0.02`）

`autoDetectBorder = false` 时的回退裁剪比例（每边）。`0.02` = 每边裁 2%。

#### `blackThreshold`（int，默认 `5`）

灰度值 < 此值视为"黑像素"。`5` 对应 `[0, 5)` 的暗部。

#### `blackRatio`（double，默认 `0.95`）

某行/列黑像素占比 ≥ 此值时，该行/列视为"黑边"。`0.95` 表示"95% 以上是黑像素才算黑边"。

---

### 五、Unsharp Mask 锐化

> **以下四个字段仅在 `qualityLevel = -1`，或 `qualityLevel >= 0` 但找不到对应 preset 时生效。**

#### `enableUnsharpMask`（bool，默认 `true`）

是否启用 Unsharp Mask 锐化。融合过程会轻微平滑细节，锐化能恢复高频。

#### `unsharpAmount`（double，默认 `0.6`）

锐化强度，`0` = 不锐化，`2` = 极强。推荐 `0.5 ~ 1.0`。

#### `unsharpRadius`（double，默认 `1.2`）

高斯模糊 sigma。`1.0 ~ 2.0` 是常规范围。越大影响范围越广。

#### `unsharpThreshold`（double，默认 `0.0`）

只在 `|原图 - 锐化图| > threshold` 的像素上应用锐化。

| 值 | 效果 |
| :---: | :--- |
| `0` | 全图锐化，**噪点也会被锐化** |
| **`1 ~ 3`** | **推荐；只在边缘锐化，噪点不动** |
| `10+` | 几乎不锐化 |

---

### 六、detailEnhance（可选）

> **`enableDetailEnhance` 仅在 `qualityLevel = -1`，或找不到对应 preset 时生效。**
> **`detailSigma_s` / `detailSigma_r` 始终生效**（但只有在 `enableDetailEnhance = true` 时才被使用）。

#### `enableDetailEnhance`（bool，默认 `false`）

是否启用 `cv::detailEnhance` 局部对比度增强。

**⚠️ 会改变色彩风格**（人像变"卡通感"，天空变深蓝），且可能产生 Halo 伪影。默认关闭，追求"真实感"时不建议开启。

#### `detailSigma_s` / `detailSigma_r`（double，默认 `10.0` / `0.15`）

detailEnhance 的参数，对应 `sigma_s`（空间域标准差）和 `sigma_r`（色域标准差）。OpenCV 示例默认 `10.0 / 0.15`。

---

### 七、调试产物

#### `saveIntermediate`（bool，默认 `true`）

是否保存 `04_hdr_merged_raw.jpg`（未锐化的原始 Mertens 结果）。关闭后只保留最终结果，节省磁盘。

---

## 完整参数速查表

| 参数 | 类型 | 默认值 | 取值范围 | 分组 | 受 qualityLevel 影响 |
| :--- | :---: | :---: | :--- | :--- | :---: |
| `qualityLevel` | int | `1` | -1 或 0~N | 档位 | — |
| `qualityPresets` | map | 3 档 | 任意 | 档位 | — |
| `contrastWeight` | double | `1.2` | 0.5~2.0 | Mertens | ✅ |
| `saturationWeight` | double | `1.1` | 0.8~1.5 | Mertens | ✅ |
| `exposureWeight` | double | `1.0` | 0.5~1.5 | Mertens | ✅ |
| `autoDetectBorder` | bool | `true` | true/false | 裁剪 | ❌ |
| `cropRatio` | double | `0.02` | 0.0~0.1 | 裁剪 | ❌ |
| `blackThreshold` | int | `5` | 0~30 | 裁剪 | ❌ |
| `blackRatio` | double | `0.95` | 0.5~1.0 | 裁剪 | ❌ |
| `enableUnsharpMask` | bool | `true` | true/false | 锐化 | ✅ |
| `unsharpAmount` | double | `0.6` | 0.0~2.0 | 锐化 | ✅ |
| `unsharpRadius` | double | `1.2` | 0.5~3.0 | 锐化 | ✅ |
| `unsharpThreshold` | double | `0.0` | 0.0~10.0 | 锐化 | ✅ |
| `enableDetailEnhance` | bool | `false` | true/false | 增强 | ✅ |
| `detailSigma_s` | double | `10.0` | 5.0~20.0 | 增强 | ❌ |
| `detailSigma_r` | double | `0.15` | 0.05~0.3 | 增强 | ❌ |
| `saveIntermediate` | bool | `true` | true/false | 调试 | ❌ |

**"受 qualityLevel 影响"的含义**：

- ✅ 当 `qualityLevel >= 0` 且 `qualityPresets` 里有对应 key 时，该字段会被 preset 里的同名值**覆盖**
- ❌ 该字段始终使用自己声明的值

---

## 完整默认 JSON

首次运行程序时自动生成：

```json
{
    "autoDetectBorder": true,
    "blackRatio": 0.95,
    "blackThreshold": 5,
    "contrastWeight": 1.2,
    "cropRatio": 0.02,
    "detailSigma_r": 0.15,
    "detailSigma_s": 10.0,
    "enableDetailEnhance": false,
    "enableUnsharpMask": true,
    "exposureWeight": 1.0,
    "qualityLevel": 1,
    "qualityPresets": {
        "0": {
            "contrastWeight": 0.8,
            "enableDetailEnhance": false,
            "enableUnsharpMask": false,
            "exposureWeight": 1.0,
            "saturationWeight": 1.0,
            "unsharpAmount": 0.6,
            "unsharpRadius": 1.2,
            "unsharpThreshold": 0.0
        },
        "1": {
            "contrastWeight": 1.2,
            "enableDetailEnhance": false,
            "enableUnsharpMask": true,
            "exposureWeight": 1.0,
            "saturationWeight": 1.1,
            "unsharpAmount": 0.6,
            "unsharpRadius": 1.2,
            "unsharpThreshold": 0.0
        },
        "2": {
            "contrastWeight": 1.5,
            "enableDetailEnhance": false,
            "enableUnsharpMask": true,
            "exposureWeight": 1.2,
            "saturationWeight": 1.2,
            "unsharpAmount": 1.0,
            "unsharpRadius": 1.5,
            "unsharpThreshold": 2.0
        }
    },
    "saturationWeight": 1.1,
    "saveIntermediate": true,
    "unsharpAmount": 0.6,
    "unsharpRadius": 1.2,
    "unsharpThreshold": 0.0
}
```

**注意**：

- `nlohmann/json` 序列化时按字段名**字母升序**排列，不是源码顺序
- `qualityPresets` 的 key 是**字符串**（`"0"` / `"1"` / `"2"`）
- 嵌套 map 内层同样按字母升序

---

## 典型调参场景

### 场景 1：HDR 结果"发灰"

**方案 A：改细粒度字段 + `qualityLevel = -1`**

```json
{
    "qualityLevel": -1,
    "contrastWeight": 1.4,
    "saturationWeight": 1.2,
    "exposureWeight": 1.1
}
```

**方案 B：改"平衡"档的参数 + `qualityLevel = 1`**

```json
{
    "qualityLevel": 1,
    "qualityPresets": {
        "1": {
            "contrastWeight": 1.4,
            "saturationWeight": 1.2,
            "exposureWeight": 1.1,
            "enableUnsharpMask": true,
            "unsharpAmount": 0.6,
            "unsharpRadius": 1.2,
            "unsharpThreshold": 0.0,
            "enableDetailEnhance": false
        }
    }
}
```

> **注意**：方案 B 里如果只写 `"1"`，其他档位（`"0"` / `"2"`）会**丢失**——因为 `qualityPresets` 是整体替换，不是字段合并。要保留其他档位，必须把 `"0"` / `"2"` 也一起写进去。

### 场景 2：亮暗边界出现光晕

**降低 `contrastWeight`**：

```json
{
    "qualityLevel": -1,
    "contrastWeight": 1.0,
    "saturationWeight": 1.0
}
```

或改 `qualityPresets` 对应档位。

### 场景 3：四角发黑

```json
{
    "autoDetectBorder": true,
    "blackThreshold": 10,
    "blackRatio": 0.90
}
```

`blackThreshold` 提高 + `blackRatio` 降低 = 更容易检测到黑边。

### 场景 4：想更"真实"的风格

```json
{
    "qualityLevel": 0,
    "enableUnsharpMask": false,
    "enableDetailEnhance": false
}
```

或直接 `qualityLevel = -1` + 手动设。

### 场景 5：想更"锐"的风格（风景/建筑）

```json
{
    "qualityLevel": -1,
    "contrastWeight": 1.4,
    "saturationWeight": 1.15,
    "enableUnsharpMask": true,
    "unsharpAmount": 1.0,
    "unsharpRadius": 1.5,
    "unsharpThreshold": 2.0
}
```

### 场景 6：新增"极致锐"档

编辑 `qualityPresets`，追加 `"3"`：

```json
"qualityPresets": {
    "0": { ... },
    "1": { ... },
    "2": { ... },
    "3": {
        "contrastWeight": 1.8,
        "saturationWeight": 1.25,
        "exposureWeight": 1.3,
        "enableUnsharpMask": true,
        "unsharpAmount": 1.3,
        "unsharpRadius": 1.8,
        "unsharpThreshold": 3.0,
        "enableDetailEnhance": false
    }
}
```

然后设 `"qualityLevel": 3`。

---

## 常见问题

### Q1：修改 JSON 后重新运行，日志显示的还是旧值

**原因**：文件编码带 UTF-8 BOM，`nlohmann/json` 解析失败，回退到默认值。

**验证**：看日志第一行：

- `[HdrConfig] Loaded: ... (qualityLevel=X, presets=N, ...)` → 加载成功
- `[HdrConfig] ... not found, creating default` → JSON 不存在或解析失败

**修复**：用 VS Code 右下角编码选择器，改成 **"UTF-8"**（不是 "UTF-8 with BOM"）。

### Q2：修改 JSON 后是否需要重新编译？

**不需要**。`HdrConfig::LoadFromFile` 在程序启动时读取 JSON。只有修改 `.h` / `.cpp` 源码时才需要 `cmake --build`。

### Q3：`qualityLevel` 和细粒度字段冲突时听谁的？

- `qualityLevel >= 0` 且 `qualityPresets` 里有对应 key → **preset 覆盖细粒度字段**
- `qualityLevel >= 0` 但 `qualityPresets` 里没有 key → **打印警告，用细粒度字段**
- `qualityLevel = -1` → **用细粒度字段**

### Q4：哪个参数影响画质最大？

| 优先级 | 参数 | 影响 |
| :---: | :--- | :--- |
| **1** | `contrastWeight` | **决定"发灰"还是"发锐"** |
| **2** | `unsharpAmount` | 决定细节锐度 |
| 3 | `saturationWeight` | 决定色彩饱和度 |
| 4 | `unsharpThreshold` | 决定噪点是否被锐化 |

### Q5：改 `qualityPresets` 时漏写某个字段会怎样？

**该字段会变成 `QualityPreset` 的默认值**（`contrastWeight=1.2` 等），不是"用 JSON 里的细粒度字段"。

**原因**：`qualityPresets` 是独立的 `QualityPreset` struct，字段独立初始化。漏写 = 用 struct 默认值。

**建议**：改某个档位时，把该档位的 8 个字段全写一遍。

### Q6：`qualityPresets` 为空 `{}` 会怎样？

`qualityLevel >= 0` 时找不到任何 key → 所有档位都退化到细粒度字段。**不会崩**，但 `qualityLevel` 就失去意义了。

### Q7：配准失败会怎样？

`AlignToReference` 内部 `try/catch`，配准失败时回退到 `cv::resize` 到参考图尺寸。**不会崩，但 HDR 结果会有"重影"**（因为没有真正对齐）。

### Q8：JSON 里 `qualityLevel` 写小数会怎样？

`qualityLevel` 是 `int`，JSON 里写 `1.5` 会**解析失败**（`nlohmann/json` 抛异常或返回 `nullopt`），回退到默认配置。

**必须写整数**。

---

## 相关文件

- `inc/HdrMerge/HdrConfig.h` — 配置结构体（`QualityPreset` + `HdrConfig`）+ `ApplyQualityLevel` + JSON 序列化
- `inc/HdrMerge/HdrMerge.h` — HDR 融合实现
- `tests/HdrMerge/HdrMergeTest.h` — 测试入口（含单次运行 + 多档位对比）
- `data/HdrMerge/{low,normal,high}.jpg` — 输入数据