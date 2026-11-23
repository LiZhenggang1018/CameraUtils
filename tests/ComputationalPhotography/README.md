# tests/ComputationalPhotography · HDR 融合测试

本目录存放 HDR 融合模块的测试入口，全部为 header-only 的 `inline` 函数，由 `tests/cli/CommandTable.h` 注册为 CLI 子命令，CLI 与 `camera_utils_tests` 共用同一张命令表。`RunHdrMergeTest()` 是 HDR 融合功能 demo 的起始点；`RunHdrMergePhysicalTest()` 是新增的物理 HDR 双 tonemap 对比测试。

---

## 1. 文件清单

| 文件 | 职责 |
| :--- | :--- |
| `HdrMergeTest.h` | `RunHdrMergeTest`、`RunHdrMergeMultiLevelTest`、`RunHdrMergePhysicalTest` |
| `HdrMergeUnitTest.h` | HDR 纯内存断言（不依赖 data/ 素材） |
| `MfnrTest.h` | MFNR 多帧降噪断言 |
| `FusionDemo.h` | 多帧融合（HDR+降噪）demo |

相关外部文件：

| 文件 | 作用 |
| :--- | :--- |
| `inc/ComputationalPhotography/HdrMerge.h` | 主流程与算法 |
| `inc/ComputationalPhotography/HdrConfig.h` | 配置结构与 JSON 读写 |
| `inc/ComputationalPhotography/Mfnr.h` | MFNR 多帧降噪 |
| `configs/HdrConfig.json` | 运行时配置，缺失时首次运行自动生成 |
| `data/HdrMerge/{low,normal,high}.jpg` | 输入数据 |
| `tests/cli/CommandTable.h` | 命令注册表，HDR 用例在此登记为 `hdr-merge*` 子命令 |

---

## 2. 入口链

```text
src/main.cpp  (或 tests/TestMain.cpp)
  └─ tests/cli/CommandTable.h
       ├─ RunHdrMergeTest()              // hdr-merge，单次 Mertens
       ├─ RunHdrMergeMultiLevelTest()    // hdr-merge-multilevel，多档位对比
       ├─ RunHdrMergePhysicalTest()      // hdr-merge-physical，Drago + Reinhard
       └─ RunHdrMergeGhostSuppressionTest() // hdr-merge-ghost，鬼影抑制
```

---

## 3. 三个测试函数

### 3.1 `RunHdrMergeTest()`

单次运行，Mertens 路径。

- 输入：`data/HdrMerge/{low,normal,high}.jpg`
- 输出：`data/HdrMerge/result/`
- 流程：打印四个路径 → `HdrConfig::LoadFromFile(DefaultPath())` → `HdrMergeFromFiles(..., cfg)` → 打印返回码。
- 前后有 `========== HdrMerge begin/end ==========` 分隔。

输出目录内容（取决于配置）：

```text
result/
  01_low_registered.jpg
  02_normal_reference.jpg
  03_high_registered.jpg
  04_hdr_merged_raw.jpg          (saveIntermediate = true)
  05_hdr_detail_enhanced.jpg     (enableDetailEnhance = true)
  hdr_merged.jpg                 (最终结果)
```

### 3.2 `RunHdrMergeMultiLevelTest()`

多档位对比，Mertens 路径。

- 输入同上。
- 先加载 `baseCfg`；`qualityPresets` 为空则打印错误并返回。
- 遍历 `baseCfg.qualityPresets` 的 `[key, preset]`：
  - `std::stoi(key)` 转整数；非整数 key 打印 "Skip non-integer key" 并 `continue`。
  - 拷贝 `baseCfg`，设 `cfg.qualityLevel = level`，调 `cfg.ApplyQualityLevel()`。
  - 输出目录 `data/HdrMerge/result_level_<key>/`。
  - 调 `HdrMergeFromFiles`，打印返回码。
- JSON 中新增 `"3"` / `"4"` 档位会被自动遍历。

输出目录：

```text
result_level_0/
result_level_1/
result_level_2/
result_level_3/    (如果 JSON 里加了 "3")
```

### 3.3 `RunHdrMergePhysicalTest()`

物理 HDR 双 tonemap 对比。

- 输入同上。
- 加载 `baseCfg` 后，遍历 `tonemapTypes = { "drago", "reinhard" }`：
  - 拷贝 `baseCfg`，设 `cfg.pipelineMode = "physical"`、`cfg.tonemapType = type`、`cfg.enableUnsharpMask = false`（先看纯 tonemap 结果）。
  - 输出目录 `data/HdrMerge/result_physical_<type>/`。
  - 调 `HdrMergeFromFiles`，打印返回码。
- 若 EXIF 缺失，主流程会打印 `no valid exposure info` 并自动回退 Mertens，返回码仍为 Mertens 路径的结果。

输出目录：

```text
result_physical_drago/
  01_low_registered.jpg
  02_normal_reference.jpg
  03_high_registered.jpg
  04_hdr_linear_32f.hdr      (saveIntermediate = true)
  hdr_merged.jpg             (Drago tonemap 结果)
result_physical_reinhard/
  ...（同上，tonemap 为 Reinhard）
```

物理路径成功时不会生成 `04_hdr_merged_raw.jpg` 和 `05_hdr_detail_enhanced.jpg`，因为这两个文件属于 Mertens 路径的中间产物。

---

## 4. 运行方式

### 4.1 Visual Studio 2022

1. 选择 `x64-debug` 或 `x64-release` 预设。
2. 生成 `CameraUtils`。
3. 直接运行，或通过“测试资源管理器”运行 `TestCameraUtils`。

### 4.2 命令行

```powershell
ctest --test-dir out/build/x64-release -C Release --output-on-failure
```

或直接运行可执行文件：

```powershell
.\out\build\x64-release\CameraUtils.exe
```

---

## 5. 配置对测试的影响

| 配置项 | 影响 |
| :--- | :--- |
| `pipelineMode` | 单次测试是否走物理 HDR；`RunHdrMergePhysicalTest` 会强制覆盖为 `physical` |
| `qualityLevel` | 单次测试使用的档位；多档位测试逐档覆盖 |
| `qualityPresets` | 多档位测试遍历的档位集合 |
| `tonemapType` | 物理路径使用 Drago 或 Reinhard；`RunHdrMergePhysicalTest` 两个都跑 |
| `readExposureFromExif` / `manualExposureTimes` / `manualIsos` / `baseIso` | 物理路径的等效曝光量来源 |
| `calibrateResponse` | 物理路径是否用 `CalibrateDebevec` 估计 CRF |
| `saveIntermediate` | 是否生成 `04_*.hdr` / `04_*_raw.jpg` |
| `enableDetailEnhance` | Mertens 路径是否生成 `05_hdr_detail_enhanced.jpg` |
| `autoDetectBorder` / `cropRatio` / `blackThreshold` / `blackRatio` | 裁剪策略，影响输出尺寸 |
| `enableUnsharpMask` 及三参数 | 最终 `hdr_merged.jpg` 的锐化程度 |

修改 `configs/HdrConfig.json` 后重新运行即可，不需要重新编译。

---

## 6. 新增测试的步骤

1. 在 `tests/HdrMerge/` 下新增 `XxxTest.h`，写 `inline void XxxTest() { ... }`。
2. 在 `tests/cli/CommandTable.h` 的 `AllCommands()` 中加一条登记项（CLI 与测试自动可见）。
3. 如需独立数据，放到 `data/HdrMerge/` 下，用 `EnvRootPath()` 拼路径。
4. 若要纳入 CTest，在 `tests/CMakeLists.txt` 的 `_media_tests` 列表追加命令名。
5. 构建并运行：`CameraUtils.exe <command>` 或 `camera_utils_tests.exe <command>`。

---

## 7. 常见问题

| 现象 | 排查方向 |
| :--- | :--- |
| 返回码 1/2/3 | `data/HdrMerge/` 下对应 jpg 缺失或路径错误 |
| 返回码 4 | Mertens 融合失败，检查输入尺寸/通道是否一致 |
| 返回码 5 | 输出目录创建失败，检查写权限 |
| 返回码 6 | ROI 无效，检查 `blackThreshold` / `blackRatio` 是否过严 |
| 输出有重影 | 配准失败回退到 `cv::resize`，查看日志中 `Align failed` 或 `poor alignment` |
| 修改 JSON 不生效 | 文件编码需 UTF-8 无 BOM；看日志首行是 `Loaded` 还是 `not found or invalid` |
| 多档位测试只跑了一档 | `qualityPresets` 里只有一个 key，或 key 非整数被跳过 |
| 物理测试日志出现 `no valid exposure info` | 三张图 EXIF 缺 `ExposureTime` 或 ISO；可改用 `manualExposureTimes` + `manualIsos` 手动配置 |
| 物理测试输出与 Mertens 几乎一样 | 检查日志 `Physical HDR succeeded` 还是 `fallback to Mertens`；若回退，按上一条排查 EXIF |
| 物理路径没有 `04_hdr_linear_32f.hdr` | `saveIntermediate = false`，或物理路径失败回退 Mertens（此时生成的是 `04_hdr_merged_raw.jpg`） |
| Drago 与 Reinhard 结果差异很小 | 三张输入动态范围本身较窄；检查输入是否真的是不同曝光 |