# LivePhotoDecode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 `LivePhotoDecode` 章节，解析 vivo LivePhoto（`input.jpg` 封面 + `IMG_*.mp4` 视频），产出封面与 77 帧视频的 YUV/JPG、封面 EXIF 曝光信息、以及随帧 meta。

**Architecture:** 复用工程现有 FFmpeg 封装（`DecAndEnc.h::DecodeMp4ToYuvFrames`）、exiv2 封装（`Exiv2Common.h`）、OpenCV YUV 读写（`YUVImageReader`）。新增 `LivePhotoDecode` 章节承担编排 + mett 轨道逆向解析。mett 是 vivo 专有随帧二进制，用封面 EXIF 真值锚定字段语义，未确证字段标注 speculative。

**Tech Stack:** C++20、FFmpeg（avformat/avcodec/swscale）、OpenCV、exiv2、nlohmann_json。

**Spec:** `docs/superpowers/specs/2026-11-25-livephoto-decode-design.md`

## Global Constraints

- 构建：`scripts\build-debug.bat <target>`（chcp 65001 + vcvars64 + ninja）。
- 新文件 UTF-8 无 BOM + LF；K&R 大括号；类型/函数大驼峰、变量小驼峰、成员 `_` 后缀、常量全大写；头文件禁止 `using namespace`；注释英文。
- 章节归档：`inc/LivePhotoDecode/` + `src/LivePhotoDecode/` + `tests/LivePhotoDecode/`。
- `src/CMakeLists.txt` 需新增 `file(GLOB ... LivePhotoDecode/*.cpp)` 加入 `camera_utils_core`，`target_include_directories` 加 `inc/LivePhotoDecode`。
- 测试归类：命令注册 `Label::MEDIA`，并加入 `tests/CMakeLists.txt` 的 `_media_tests`。
- 命令名：`livephoto-decode`（入口函数 `RunLivePhotoDecodeTest()`）。
- 单文件提交 + 单文件 push（`git add <单文件>` → `commit` → `push origin main`）。
- 头文件 include guard：`INC_LIVEPHOTODECODE_<文件名>_H` / `TESTS_LIVEPHOTODECODE_<文件名>_H`。

## Review Focus

| 输入/失效模式 | 期望行为 | 测试位置 |
|---|---|---|
| `input.jpg` / `IMG_*.mp4` 不存在 | 打印明确错误并返回非 0，不崩溃 | Task 1（路径检查） |
| mett 轨道缺失或 packet 数 ≠ 视频帧数 | 打印警告，时间戳仍可输出，meta 字段标 -1 | Task 4 |
| 封面 EXIF 缺曝光/ISO 字段 | 输出 -1 并标注，不抛异常 | Task 2 |
| 视频帧含 rotation=-90（竖拍） | 逐帧 JPG 尺寸仍正确写出（沿用 DecodeMp4ToYuvFrames，无需旋转） | Task 3 |
| mett packet 内 float 偏移非 4 对齐 | 按 big-endian 逐 4 字节扫描，容忍非对齐 | Task 4 |

---

### Task 1: 章节骨架 + 路径常量 + 命令注册

**Files:**
- Create: `inc/LivePhotoDecode/LpDecodeDefine.h`
- Create: `inc/LivePhotoDecode/README.md`
- Create: `tests/LivePhotoDecode/LivePhotoDecodeTest.h`
- Create: `tests/LivePhotoDecode/README.md`
- Modify: `tests/cli/CommandTable.h`（加 include + 命令项）
- Modify: `tests/CMakeLists.txt`（`_media_tests` 加 `livephoto-decode`）
- Modify: `src/CMakeLists.txt`（glob + include dir）
- Modify: `src/LivePhotoSmoothPlay/` 无关，不动

**Interfaces:**
- Produces: `std::string LivePhotoDecodeDataPath()` → `EnvRootPath() + "/data/LivePhotoDecode/"`；`std::string LpInputJpgPath()` → `... + "input.jpg"`；`std::string LpInputMp4Path()` → `... + "IMG_20261125_112849.mp4"`；`std::string LpOutputDir()` → `... + "output"`。均为函数内 `static` 返回（沿用 `FFmpegDefine.h` 模式，避免静态初始化顺序问题）。
- Produces: `void RunLivePhotoDecodeTest()`（`tests/LivePhotoDecode/LivePhotoDecodeTest.h` 中 `inline`）。

- [ ] **Step 1: 写 `inc/LivePhotoDecode/LpDecodeDefine.h`**

  四个路径函数（函数内 static），include `<string>` 与 `"CommonEnv.h"`。include guard `INC_LIVEPHOTODECODE_LPDECODEDEFINE_H`。

- [ ] **Step 2: 写 `tests/LivePhotoDecode/LivePhotoDecodeTest.h`（骨架版）**

  ```cpp
  inline void RunLivePhotoDecodeTest()
  {
      std::cout << "[LivePhotoDecode] input.jpg = " << LpInputJpgPath() << std::endl;
      std::cout << "[LivePhotoDecode] mp4      = " << LpInputMp4Path() << std::endl;
      std::cout << "[LivePhotoDecode] output   = " << LpOutputDir() << std::endl;
      // 路径存在性检查：缺失则打印错误并 return
  }
  ```
  include `"LivePhotoDecode/LpDecodeDefine.h"`，`<filesystem>`，`<iostream>`。

- [ ] **Step 3: 注册命令与 CMake 接线**

  - `CommandTable.h`：加 `#include "LivePhotoDecode/LivePhotoDecodeTest.h"`，加命令项 `{ .name = "livephoto-decode", .description = "解析 vivo LivePhoto（封面+视频帧+EXIF+随帧meta）", .label = Label::MEDIA, .run = [] { RunLivePhotoDecodeTest(); return 0; } }`。
  - `tests/CMakeLists.txt`：`_media_tests` 列表加 `livephoto-decode`。
  - `src/CMakeLists.txt`：加 `file(GLOB LIVE_PHOTO_DECODE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/LivePhotoDecode/*.cpp")`，加入 `camera_utils_core` 源列表，`target_include_directories` 加 `${CMAKE_SOURCE_DIR}/inc/LivePhotoDecode`。

- [ ] **Step 4: 构建验证**

  Run: `scripts\build-debug.bat camera_utils_tests`
  Expected: 编译通过。

- [ ] **Step 5: 运行验证**

  Run: `out\build\x64-debug\tests\camera_utils_tests.exe livephoto-decode`
  Expected: 打印三个路径，均存在。

- [ ] **Step 6: 提交（逐文件）**

  按单文件规则依次 add/commit/push：`LpDecodeDefine.h`、`LivePhotoDecodeTest.h`、`CommandTable.h`、`tests/CMakeLists.txt`、`src/CMakeLists.txt`、两个 README。

---

### Task 2: 封面帧 YUV/JPG 提取 + 封面 EXIF 曝光信息

**Files:**
- Create: `inc/LivePhotoDecode/LpDecode.h`
- Modify: `tests/LivePhotoDecode/LivePhotoDecodeTest.h`
- Create: `src/LivePhotoDecode/LpDecode.cpp`

**Interfaces:**
- Consumes: `LpInputJpgPath()`、`LpOutputDir()`（Task 1）。
- Consumes: `CameraUtils::ReadExposureInfo(const std::string&)`（`Exiv2Common.h`，返回 `ExposureInfo{timeSeconds, iso}`）。
- Consumes: `YUVImageReader::SaveAsYuv420P(const cv::Mat&, const std::string&)`。
- Produces: `void ExtractCoverFrame()` —— 读封面 JPG、写 `output/cover.jpg` 与 `output/cover.yuv`。
- Produces: `void ExtractCoverExif()` —— 读 EXIF，写 `output/cover_exif.txt`（含 ExposureTime、ISO、DateTimeOriginal）。

- [ ] **Step 1: 写 `inc/LivePhotoDecode/LpDecode.h`**

  声明两个函数：`void ExtractCoverFrame();`、`void ExtractCoverExif();`，include `"LivePhotoDecode/LpDecodeDefine.h"`。guard `INC_LIVEPHOTODECODE_LPDECODE_H`。

- [ ] **Step 2: 实现 `src/LivePhotoDecode/LpDecode.cpp` 的 `ExtractCoverFrame`**

  用 OpenCV `cv::imread(LpInputJpgPath())` 读封面；空则打印错误返回。`cv::imwrite(LpOutputDir()+"/cover.jpg", img)`；用 `YUVImageReader(img.cols, img.rows).SaveAsYuv420P(img, LpOutputDir()+"/cover.yuv")`。先 `std::filesystem::create_directories(LpOutputDir())`。

- [ ] **Step 3: 实现 `ExtractCoverExif`**

  调 `CameraUtils::ReadExposureInfo(LpInputJpgPath())` 拿曝光时间/ISO；用 exiv2 直接读 `Exif.Photo.DateTimeOriginal`（`Exiv2::ImageFactory::open` + `exifData().findKey`）。写 `output/cover_exif.txt`，字段缺失写 `-1`/空串。catch `Exiv2::Error`。

- [ ] **Step 4: 更新测试入口调用**

  `RunLivePhotoDecodeTest()` 依次调 `ExtractCoverFrame(); ExtractCoverExif();`。

- [ ] **Step 5: 构建 + 运行验证**

  Run: `scripts\build-debug.bat camera_utils_tests`；`camera_utils_tests.exe livephoto-decode`
  Expected: 生成 `output/cover.jpg`（3072×4096）、`cover.yuv`（大小 = 3072*4096*3/2）、`cover_exif.txt` 含曝光/ISO/时间戳（时间戳约 2026:11:25 11:28:49）。

- [ ] **Step 6: 提交**

---

### Task 3: live 流逐帧 YUV/JPG 提取

**Files:**
- Modify: `inc/LivePhotoDecode/LpDecode.h`
- Modify: `src/LivePhotoDecode/LpDecode.cpp`
- Modify: `tests/LivePhotoDecode/LivePhotoDecodeTest.h`

**Interfaces:**
- Consumes: `LpInputMp4Path()`、`LpOutputDir()`（Task 1）。
- Consumes: `DecodeMp4ToYuvFrames(const char* inFilename, const std::string& outDir)`（`LivePhotoSmoothPlay/DecAndEnc.h`，解码 mp4 → `{N}.yuv` + `{N}.jpg`）。
- Produces: `void ExtractLiveStream()` —— 解码视频到 `output/video/`，产出 `001.yuv..077.yuv` + `001.jpg..077.jpg`。

- [ ] **Step 1: 声明 `void ExtractLiveStream()`**

- [ ] **Step 2: 实现**

  调 `DecodeMp4ToYuvFrames(LpInputMp4Path().c_str(), LpOutputDir()+"/video")`。注意该函数输出从 `1.yuv` 起（非零填充），实现里在调用后可选重命名，或直接在文档注明命名从 `1` 起。保持函数原样，不改 `DecAndEnc.h`。

- [ ] **Step 3: 更新测试入口调用 `ExtractLiveStream()`**

- [ ] **Step 4: 构建 + 运行验证**

  Run: `scripts\build-debug.bat camera_utils_tests`；`camera_utils_tests.exe livephoto-decode`
  Expected: `output/video/` 下有 77 个 `.yuv` 与 77 个 `.jpg`（帧数 = `ffprobe nb_frames=77`），单个 yuv 大小 = 1728*1296*3/2。

- [ ] **Step 5: 提交**

---

### Task 4: mett 随帧 meta 解析 + 字段锚定

**Files:**
- Create: `inc/LivePhotoDecode/LpMetaParser.h`
- Create: `src/LivePhotoDecode/LpMetaParser.cpp`
- Modify: `inc/LivePhotoDecode/LpDecode.h`
- Modify: `src/LivePhotoDecode/LpDecode.cpp`
- Modify: `tests/LivePhotoDecode/LivePhotoDecodeTest.h`

**Interfaces:**
- Consumes: `LpInputMp4Path()`、`LpOutputDir()`。
- Consumes: 封面 EXIF 真值（`CameraUtils::ReadExposureInfo`）。
- Produces:
  ```cpp
  struct MettPacket {
      int64_t pts;                    // mett 流 time_base (1/90000)
      std::vector<uint8_t> data;
  };
  std::vector<MettPacket> ReadMettPackets(const std::string& mp4Path);  // 读 stream 0（codec_tag 0x7474656d）
  std::vector<float> ParseBigEndianFloats(const std::vector<uint8_t>& data); // 逐 4 字节 big-endian 扫描 IEEE754
  struct FrameMeta {
      int frameIndex;      // 1-based
      double ptsSeconds;   // pts / 90000
      double exposureTime; // speculative，-1 未知
      double iso;          // speculative，-1 未知
      std::vector<float> rawFloats;
  };
  std::vector<FrameMeta> BuildFrameMeta(const std::vector<MettPacket>&, double coverExposure, double coverIso);
  void ExtractFrameMeta();  // 写 output/frame_meta.txt
  ```

- [ ] **Step 1: 写 `inc/LivePhotoDecode/LpMetaParser.h`**

  声明上述 struct 与函数。纯函数 `ParseBigEndianFloats` 与 `BuildFrameMeta` 放头文件 `inline`（可单测）；`ReadMettPackets`、`ExtractFrameMeta` 在 `.cpp` 实现。guard `INC_LIVEPHOTODECODE_LPMETAPARSER_H`。

- [ ] **Step 2: 实现 `ParseBigEndianFloats`（inline，可单测）**

  输入字节序列，从 offset 0 起每 4 字节按 big-endian 拼成 `float`（`std::bit_cast<float>(uint32)` 前先 `__builtin_bswap32` / `_byteswap_ulong`），过滤掉 NaN/Inf 与 `abs > 1e6` 的噪声值，保留其余。

- [ ] **Step 3: 实现 `ReadMettPackets`（.cpp）**

  用 `avformat_open_input` 打开 mp4，`avformat_find_stream_info`，遍历流找 `codecpar->codec_type == AVMEDIA_TYPE_DATA` 且 `codec_tag == 0x7474656d` 的 stream 0。`av_read_frame` 循环，仅收该 stream_index 的 packet，拷贝 `pts` 与 `data`（`av_packet_unref` 前 memcpy 到 vector）。RAII 用 `AVFormatContextPtr`/`AVPacketPtr`（`FFmpegDefine.h` 已提供）。记录该流的 `time_base` 用于换算。

- [ ] **Step 4: 实现 `BuildFrameMeta`（inline，可单测）**

  对每个 packet：`ptsSeconds = pts * time_base.num / (double)time_base.den`（time_base 1/90000 → pts/90000）；`rawFloats = ParseBigEndianFloats(data)`；在 `rawFloats` 里找与 `coverExposure`（±2% 相对容差）最接近的值作为 `exposureTime`，找与 `coverIso`（±2%）最接近的作为 `iso`，找不到则 -1。`frameIndex` 按 packet 顺序 1..N。

- [ ] **Step 5: 实现 `ExtractFrameMeta`（.cpp）**

  读 `coverExposure`/`coverIso`（`ReadExposureInfo`），`ReadMettPackets` → `BuildFrameMeta`，写 `output/frame_meta.txt`，每行 `index pts_sec exposure_time iso`，iso 后附 `raw=[...]` 未识别值（标注 speculative）。packet 数 ≠ 视频帧数时打印警告。

- [ ] **Step 6: 更新测试入口调用 `ExtractFrameMeta()`**

- [ ] **Step 7: 构建 + 运行验证**

  Run: `scripts\build-debug.bat camera_utils_tests`；`camera_utils_tests.exe livephoto-decode`
  Expected: 生成 `output/frame_meta.txt`，77 行，第 1 帧的 `exposure_time`/`iso` 与 `cover_exif.txt` 锚定值一致（±2%）。

- [ ] **Step 8: 提交**

---

### Task 5: 文档收尾

**Files:**
- Modify: `inc/LivePhotoDecode/README.md`（接口说明）
- Modify: `tests/LivePhotoDecode/README.md`（运行说明 + 输入路径）
- Modify: `README.md`（可选：命令速查表加一行 `livephoto-decode`）

- [ ] **Step 1: 补全 `inc/LivePhotoDecode/README.md`**

  章节概述、文件结构表、四个子步骤、输出布局、mett 逆向方法边界（字段标注 speculative）。

- [ ] **Step 2: 补全 `tests/LivePhotoDecode/README.md`**

  运行命令、输入路径、预期输出。

- [ ] **Step 3: README 命令速查表加一行（如已存在该表）**

- [ ] **Step 4: 乱码校验 + 提交**

  Run: `powershell -ExecutionPolicy Bypass -File scripts/check-encoding.ps1`
  Expected: `OK: no garbled text found`。
