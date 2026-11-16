# inc/HdrMerge · HDR 融合模块头文件

本目录存放 HDR 融合模块（命名空间 `hdr_merge`）的全部头文件。模块为 header-only，无对应 `.cpp`。配置与算法分离：`HdrConfig.h` 负责配置定义与 JSON 读写，`HdrMerge.h` 负责配准、裁剪、融合、色调映射的完整流程。

当前支持**双管线**：

- `pipelineMode = "mertens"`（默认）：sRGB 域 Mertens 曝光融合，向后兼容。
- `pipelineMode = "physical"`：EXIF 曝光 + De-gamma + MergeDebevec + 色调映射，输出物理 HDR 辐射图。

物理管线失败（曝光信息缺失、MergeDebevec 或 Tonemap 失败）时自动回退 Mertens，主流程返回码不变。

---

## 1. 文件清单

| 文件 | 职责 |
| :--- | :--- |
| `HdrConfig.h` | `QualityPreset`、`HdrConfig`、档位应用、JSON 序列化 |
| `HdrMerge.h` | 配准、黑边裁剪、Mertens、物理 HDR、Unsharp Mask、主入口 |

---

## 2. `HdrConfig.h`

### 2.1 `QualityPreset`

单个质量档位的参数集合，8 个字段：

| 字段 | 类型 | 默认值 | 含义 |
| :--- | :--- | :---: | :--- |
| `contrastWeight` | double | 1.2 | Mertens 对比度权重 |
| `saturationWeight` | double | 1.1 | Mertens 饱和度权重 |
| `exposureWeight` | double | 1.0 | Mertens 曝光权重 |
| `enableUnsharpMask` | bool | true | 是否启用 Unsharp Mask |
| `unsharpAmount` | double | 0.6 | 锐化强度 |
| `unsharpRadius` | double | 1.2 | 高斯 sigma |
| `unsharpThreshold` | double | 0.0 | 锐化阈值（0 = 全图锐化） |
| `enableDetailEnhance` | bool | false | 是否启用 detailEnhance |

序列化使用 `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`，JSON 缺失字段时使用类内默认值。

### 2.2 `HdrConfig` 字段分组

**管线与档位**

| 字段 | 默认值 | 说明 |
| :--- | :---: | :--- |
| `pipelineMode` | `"mertens"` | `"mertens"` 或 `"physical"` |
| `qualityLevel` | 1 | -1 自定义；>=0 查 `qualityPresets` |
| `qualityPresets` | 三档 `"0"/"1"/"2"` | `std::map<std::string, QualityPreset>` |

**Mertens 权重（受档位覆盖）**

| 字段 | 默认值 |
| :--- | :---: |
| `contrastWeight` | 1.2 |
| `saturationWeight` | 1.1 |
| `exposureWeight` | 1.0 |

**黑边裁剪（始终生效）**

| 字段 | 默认值 | 说明 |
| :--- | :---: | :--- |
| `autoDetectBorder` | true | 是否自动检测黑边 |
| `cropRatio` | 0.02 | 固定裁剪时每边比例 |
| `blackThreshold` | 5 | 灰度 < 此值视为黑像素 |
| `blackRatio` | 0.95 | 黑像素占比 >= 此值判为黑边 |

**Unsharp Mask（受档位覆盖）**

| 字段 | 默认值 |
| :--- | :---: |
| `enableUnsharpMask` | true |
| `unsharpAmount` | 0.6 |
| `unsharpRadius` | 1.2 |
| `unsharpThreshold` | 0.0 |

**detailEnhance**

| 字段 | 默认值 | 受档位覆盖 |
| :--- | :---: | :---: |
| `enableDetailEnhance` | false | 是 |
| `detailSigma_s` | 10.0 | 否 |
| `detailSigma_r` | 0.15 | 否 |

**物理 HDR**

| 字段 | 默认值 | 说明 |
| :--- | :---: | :--- |
| `readExposureFromExif` | true | 是否从 EXIF 读取曝光时间与 ISO |
| `manualExposureTimes` | 空 | EXIF 失败时的手动回退，需 3 个秒值 |
| `manualIsos` | 空 | 与 `manualExposureTimes` 成对配置，3 个 ISO 值 |
| `baseIso` | 100.0 | 等效曝光量归一化基准 |
| `calibrateResponse` | false | 是否用 `CalibrateDebevec` 估计 CRF |
| `tonemapType` | `"drago"` | `"drago"` 或 `"reinhard"` |
| `dragoGamma` | 1.0 | TonemapDrago gamma |
| `dragoSaturation` | 1.0 | TonemapDrago 饱和度 |
| `reinhardGamma` | 1.0 | TonemapReinhard gamma |
| `reinhardIntensity` | 0.0 | 0 = 自动 |
| `reinhardLightAdapt` | 0.0 | 0 = 自动 |

**调试**

| 字段 | 默认值 |
| :--- | :---: |
| `saveIntermediate` | true |

### 2.3 关键成员函数

- `DefaultPath()`：返回 `EnvRootPath() + "/configs/HdrConfig.json"`，函数内 `static` 规避静态初始化顺序问题。
- `ApplyQualityLevel()`：
  - `qualityLevel < 0` → 直接返回，细粒度字段原样生效。
  - 否则按 `std::to_string(qualityLevel)` 查 `qualityPresets`。
  - 找到 → 用 preset 覆盖 8 个受档位影响的字段。
  - 找不到 → `fprintf(stderr, ...)` 警告，退回细粒度字段，不崩。
- `LoadFromFile(path)`：
  - `Dsrlz<HdrConfig>` 成功 → `ApplyQualityLevel()`，打印加载摘要（含 `pipeline`、`tonemap`）。
  - 失败 → 打印 "not found or invalid, creating default"，构造默认配置并 `Srlz` 写回磁盘。
- `SaveToFile(path)`：`Srlz(*this, path)`。

---

## 3. `HdrMerge.h`

命名空间 `hdr_merge`，全部为 `inline` 函数。按职责分为 7 组。

### 3.1 配准

**`AlignToReference(reference, target, registrator, detector=ORB, transform=HOMOGRAPHY)`**

- 调 `registrator.registerImages(...)` 把 target 对齐到 reference。
- 三层回退：空图 → resize；异常 → resize；质量差（`num_inliers < 30` 或 `rms_error > 5.0`）→ 只警告不回退。
- 始终保证返回值尺寸 = `reference.size()`，这是三张图共用同一个 ROI 的前提。

### 3.2 黑边裁剪

**`DetectContentRoi(img, blackThreshold=5, blackRatio=0.95)`**

- 转灰度，逐行 `ptr` 连续扫描、逐列 `at` 跳行扫描，统计黑像素占比。
- 占比 >= `blackRatio` 判为黑边，四方向独立收缩。
- 收缩交叉时返回全图 `Rect(0,0,W,H)`，避免空 ROI。

**`ComputeCommonRoi(alignedLow, alignedHigh, reference, cfg)`**

- `autoDetectBorder == false` → 按 `cropRatio` 固定裁剪每边。
- 否则对 low/high 分别检测 ROI，取**并集**外接矩形。
- 并集面积占比 < 25% → 打印警告并回退固定裁剪。
- `normal` 不参与检测，直接用该矩形裁剪。

### 3.3 Mertens 融合

**`MergeMertens(images, cfg)`**

- `cv::createMergeMertens()`，设置 contrast/saturation/exposure 三权重。
- `process` 得 `CV_32FC3 [0,1]`，`convertTo` 到 `CV_8UC3`。

### 3.4 Unsharp Mask

**`UnsharpMask(src, amount, radius, threshold)`**

- `amount <= 0` → 返回 `src.clone()`。
- `GaussianBlur(Size(0,0), radius)` + `addWeighted(src, 1+amount, blurred, -amount, 0)`。
- `threshold > 0` → `absdiff` 转灰度，`compare > threshold` 得 mask，只在 mask 区域应用锐化。
- `threshold == 0` → 全图锐化。

### 3.5 物理 HDR 基础函数

**`BuildSrgbDegammaLUT()`**

- 构建 256 项 `CV_32FC1` LUT，分段函数：`v <= 0.04045` 用 `v/12.92`，否则 `pow((v+0.055)/1.055, 2.4)`。
- 一次构建、逐像素查表，避免每像素 `pow`。

**`SrgbToLinear(bgr8u, lut)`**

- `cv::LUT` 对三通道逐通道查表，输出 `CV_32FC3 [0,1]`。

**`ResolveEffectiveExposures(lowPath, normalPath, highPath, cfg)`**

- 按 `readExposureFromExif` 读三帧 EXIF，得到 `timeSeconds` 与 `iso`。
- 三帧都有效 → 用 EXIF；否则若 `manualExposureTimes` 与 `manualIsos` 均为 3 个值 → 用手动配置。
- 都不可用 → 返回空 `vector`，调用方回退 Mertens。
- 计算 `effectiveExposure = timeSeconds * iso / baseIso`。
- `baseIso` 只影响绝对尺度，不影响辐射图相对结构，因为 `MergeDebevec` 只关心向量内部比例。

**`TonemapHdr(hdr32f, cfg)`**

- `tonemapType == "reinhard"` → `createTonemapReinhard(gamma, intensity, lightAdapt, 0)`。
- 否则 → `createTonemapDrago(gamma, saturation)`；未知类型打印警告并回退 Drago。
- 输出 `CV_8UC3`，`convertTo` 时 `alpha=255.0`。

**`MergePhysicalHdr(lowCropped, normalCropped, highCropped, lowPath, normalPath, highPath, cfg, outHdr32f, outLdr8u)`**

- 返回码：0 成功，1 曝光信息不可用，2 MergeDebevec 失败，3 Tonemap 失败。
- 流程：
  1. `ResolveEffectiveExposures` 得等效曝光量。
  2. De-gamma 三张裁剪图为线性浮点。
  3. `MergeDebevec` 合成 `CV_32FC3` HDR；`calibrateResponse` 为真时先用 `CalibrateDebevec` 估计响应。
  4. `TonemapHdr` 得 LDR。

### 3.6 主入口 `HdrMergeFromFiles`

两个重载：

- `HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg)`
- `HdrMergeFromFiles(lowPath, normalPath, highPath, outDir)`：内部 `LoadFromFile` 后委托。

带 cfg 版本流程：

1. `cv::imread` 三张图，任一为空返回 1/2/3。
2. 以 `normal` 为参考，`AlignToReference` 对齐 low、high。
3. `ComputeCommonRoi` 得 ROI，`area <= 0` 返回 6。
4. 按 ROI 裁剪三张图。
5. **分流**：
   - `pipelineMode == "physical"` → `MergePhysicalHdr`；成功则 `finalResult = ldr8u`，失败则打印警告并继续走 Mertens。
   - 否则或物理失败 → `MergeMertens`，空返回 4；`enableDetailEnhance` 为真时 `cv::detailEnhance`。
6. `enableUnsharpMask` 为真时对 `finalResult` 做 `UnsharpMask`（两条路径共用）。
7. `create_directories(outDir)`，失败返回 5。
8. 写文件（见下表）。

### 3.7 输出文件

| 文件 | 条件 |
| :--- | :--- |
| `01_low_registered.jpg` | 始终 |
| `02_normal_reference.jpg` | 始终 |
| `03_high_registered.jpg` | 始终 |
| `04_hdr_linear_32f.hdr` | 物理路径 + `saveIntermediate` |
| `04_hdr_merged_raw.jpg` | Mertens 路径 + `saveIntermediate` |
| `05_hdr_detail_enhanced.jpg` | Mertens 路径 + `enableDetailEnhance` |
| `hdr_merged.jpg` | 始终（最终结果） |

### 3.8 返回码

| 码 | 含义 |
| :---: | :--- |
| 0 | 成功（含物理失败回退 Mertens 后成功） |
| 1 | low 读取失败 |
| 2 | normal 读取失败 |
| 3 | high 读取失败 |
| 4 | Mertens 融合返回空 |
| 5 | 输出目录创建失败 |
| 6 | ROI 面积 <= 0 |

物理 HDR 失败不新增返回码，回退 Mertens 后按 Mertens 结果返回。

---

## 4. 依赖关系

```text
HdrMerge.h
  ├─ HdrConfig.h
  │    ├─ JsonUtils.h      (Srlz / Dsrlz)
  │    └─ CommonEnv.h      (EnvRootPath)
  ├─ OpenCVCommon.h        (cv::MergeMertens / MergeDebevec / Tonemap / LUT ...)
  ├─ LivePhotoSmoothPlay/ImageRegistrator.h   (配准器)
  ├─ Exiv2Common.h         (ReadExposureInfo)
  └─ CommonEnv.h
```

OpenCV 依赖的模块：`core`、`imgproc`、`photo`。`photo` 模块提供 `MergeMertens`、`MergeDebevec`、`CalibrateDebevec`、`TonemapDrago`、`TonemapReinhard`、`detailEnhance`。

---

## 5. 配置生效链路

**Mertens 路径**

1. `LoadFromFile(DefaultPath())` 读 `configs/HdrConfig.json`。
2. `ApplyQualityLevel()` 用 preset 覆盖细粒度字段。
3. `MergeMertens` 消费三权重。
4. 可选 `detailEnhance` → 可选 `UnsharpMask`。

**物理 HDR 路径**

1. 同上的配置加载与档位覆盖。
2. `ResolveEffectiveExposures` 读 EXIF 或手动配置，计算等效曝光量。
3. `BuildSrgbDegammaLUT` + `SrgbToLinear` 线性化。
4. `MergeDebevec` 合成 32F 辐射图。
5. `TonemapHdr` 映射回 8-bit。
6. 可选 `UnsharpMask`。

修改 JSON 不需要重新编译，重新运行即可。物理路径在 EXIF 缺失时会打印 `no valid exposure info` 并回退 Mertens。

---

## 6. 扩展指引

- **新增档位**：在 `configs/HdrConfig.json` 的 `qualityPresets` 加 `"3": {...}`，设 `qualityLevel = 3`。
- **新增配置字段**：在 `HdrConfig` 或 `QualityPreset` 加成员并加入序列化宏；受档位影响的字段同步在 `ApplyQualityLevel()` 中覆盖。
- **新增融合算法**：在 `HdrMergeFromFiles` 的分流处增加 `pipelineMode` 分支，保持失败回退 Mertens。
- **新增 tonemap**：在 `TonemapHdr` 中增加 `tonemapType` 分支，未知类型保留回退 Drago。
- **新增处理阶段**：在步骤 5/6 之间插入，并在 `HdrConfig` 中加开关字段。

---

## 7. 已知注意点

1. **`qualityPresets` 是整体替换而非字段合并**：JSON 里只写 `"1"` 会让 `"0"/"2"` 消失，因为 `std::map` 反序列化替换整个 map。
2. **preset 内漏写字段用 struct 默认值**，不是细粒度字段值。
3. **物理路径依赖 EXIF 完整性**：三帧中任一缺 `ExposureTime` 或 ISO，整体回退 Mertens。
4. **ISO 与增益的线性假设**：手机平台高 ISO 下实际增益可能偏离线性，当前按 EXIF ISO 线性计算；若发现高 ISO 帧融合异常，需要从 MakerNotes 读实际增益或改用内容估计。
5. **`.hdr` 输出格式**：Radiance HDR，OpenCV 内建支持，不依赖 OpenEXR。
6. **`NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` 需要 nlohmann_json >= 3.11**。