# LivePhotoDecode 特性设计文档

日期：2026-11-25
状态：待批准

## 1. 意图与动机

用户有一张 vivo X200S 拍摄的 LivePhoto（动态照片），它在手机相册里对应两个文件：

- `data/LivePhotoDecode/input.jpg` —— 封面照片（3072×4096，内嵌 EXIF）
- `data/LivePhotoDecode/IMG_20261125_112849.mp4` —— live 视频流（6.66MB，标准 MP4）

目标是解析这张 LivePhoto，产出可复用的解码能力：

1. 提取封面帧的 YUV 与 JPG；
2. 提取 live 流（视频帧）每帧的 YUV 与 JPG；
3. 从封面 EXIF 解码曝光参数与时间戳；
4. 从 live 流提取随帧 meta（曝光参数与时间戳）。

这是「学习 + 可复用」性质的特性：既是逆向分析练习，也为后续 LivePhoto 相关算法（对齐现有 `LivePhotoSmoothPlay` 章节）提供真实输入源。

## 2. 目标 / 非目标 / 反目标

### 目标

- 新增 `LivePhotoDecode` 章节（`inc/LivePhotoDecode/` + `src/LivePhotoDecode/` + `tests/LivePhotoDecode/`），注册 `Label::MEDIA` 命令，可命令行运行。
- 解码出封面 YUV（YUV420P）+ JPG，落到 `data/LivePhotoDecode/output/`。
- 解码出 live 流 77 帧的逐帧 YUV + JPG。
- 从封面 EXIF 读出曝光时间、ISO、`DateTimeOriginal` 时间戳。
- 从 `mett` 轨道解析 77 帧随帧 meta，输出帧索引 / PTS 时间戳 / 曝光相关字段，字段语义用封面 EXIF 真值锚定，对不上的字段保留原始值并标注「推测」。

### 非目标

- 不做完整 HEVC 编解码器（复用工程现有 FFmpeg 封装）。
- 不做 mett 格式的 100% 全字段逆向（仅解出与曝光/时间相关的字段）。
- 不做 GUI 展示（不注册 `Label::GUI`）。

### 反目标

- 不为了「猜字段」而无中生有地给 mett 字段命名；未确证的字段必须显式标注。
- 不依赖外部闭源工具；全部用工程已链接的 FFmpeg / OpenCV / exiv2。

## 3. 约束

- Windows 11 + PowerShell 5.1；构建统一走 `scripts\build-debug.bat`（chcp 65001 + vcvars64 + ninja）。
- 工程已链接 FFmpeg（含 `avformat` / `avcodec` / `swscale`）、OpenCV（含 contrib）、exiv2。
- 编码规范：新文件 UTF-8 无 BOM + LF；K&R 大括号；类型/函数大驼峰、变量小驼峰、成员变量 `_` 后缀、常量全大写；头文件禁止 `using namespace`；注释英文。
- 测试归类：依赖 `data/` 素材的用例必须归 `Label::MEDIA`（进 `_media_tests`，CI 无素材时 `[SKIP] return 77`）。
- 章节归档：`inc/<章节>/` + `src/<章节>/` + `tests/<章节>/`；`src/CMakeLists.txt` 需新增 `file(GLOB ... LivePhotoDecode/*.cpp)` 并加入 `camera_utils_core`，`include_directories` 需加 `inc/LivePhotoDecode`。

## 4. 输入文件结构（已实测）

### input.jpg（2,275,009 字节，vivo 专有 LivePhoto 容器）

| offset | 内容 |
|---|---|
| 0 | SOI |
| 2 | APP1 EXIF（len 42459，含内嵌 128×176 缩略图） |
| 42463 | APP2 ICC Profile |
| 43029 | APP3..APP14（FF E5~FF EC，各 65406 字节，vivo 专有视频代理，非 MP4） |
| 697247 | SOF0：主图 3072×4096 |
| 697698 | SOS 主图扫描数据 |
| 2274628 | EOI |
| 2274649 | FF FE 段（len 767，vivo 尾部元数据，含 `streamdata/streaminfo/streamcount` 与 JSON） |

封面 EXIF 含：曝光时间、ISO、`DateTimeOriginal = 2026:11:25 11:28:49`。

### IMG_20261125_112849.mp4（标准 MP4，3 流）

| 流 | 类型 | 参数 |
|---|---|---|
| 0 | `mett` 数据轨道（`codec_tag=0x7474656d`） | 77 packet = 视频帧数，随帧 meta |
| 1 | AAC 音频 | 16kHz 立体声 |
| 2 | HEVC 视频 | 1728×1296，77 帧，~2.94s，time_base 1/90000，rotation=-90 |

## 5. 方案（Approaches）

### 方案 A：FFmpeg 解码视频 + exiv2 解 EXIF + mett 字段锚定（选定）

- 需求 1/2：复用 `DecAndEnc.h::DecodeMp4ToYuvFrames`（解码 mp4 → `{N}.yuv` + `{N}.jpg`）解码 HEVC 流；封面 JPG 用 OpenCV `imread` + `imwrite`，封面 YUV 用 `YUVImageReader::SaveAsYuv420P` 生成。
- 需求 3：复用 `Exiv2Common.h::ReadExposureInfo` + `Exif2Test.h::GetExifValue` 读 EXIF。
- 需求 4：用 FFmpeg 读取 `mett` 轨道（stream 0）的 77 个 packet 原始字节，每个 packet 解析 float 数组；用封面 EXIF 曝光时间/ISO 做真值锚定，反推字段偏移；时间戳直接取视频帧 PTS（time_base 1/90000，转秒）。

### 方案 B：视频解码 + EXIF，mett 只原样 dump

- 需求 4 退化为「逐 packet 原始字节 dump + PTS 时间戳」，不做字段级逆向。
- 优点：零逆向风险。缺点：需求 4 的「曝光参数」没解出来，不满足用户诉求。

### 方案 C：全力逆向 mett 全字段

- 力求给每个 float 命名。优点：最完整。缺点：无公开文档，字段语义无法确证，易产出错误结论，工作量大且风险高。

**选择 A**：它用封面 EXIF 真值锚定，把「能确证」和「推测」分开，兼顾可靠性与用户对「曝光参数」的诉求，风险可控。

## 6. mett 逆向方法（关键）

1. 用 FFmpeg `av_read_frame` 读 stream 0 的 77 个 packet，收集 `(pts, data, size)`。
2. packet 字节序为大端（MP4 规范）。首 packet 头部观察到 `00 0F A0 0A`（疑似 4 字节 tag）后接长度、类型、`3F 80 00 00`(=1.0f) 与多个 IEEE754 float32。
3. 把 packet 里所有 4 字节对齐的 float32 序列提取出来，与封面 EXIF 真值（曝光时间秒数、ISO）比对，命中 ±容差的字段即锚定为对应语义。
4. 其余帧套用同一偏移，输出「帧号 / pts 秒 / 曝光时间(推测) / ISO(推测) / 原始未识别值」。

**明确边界**：字段名是「锚定 + 推测」，未确证字段标注 `(speculative)`。

## 7. 输出布局

```
data/LivePhotoDecode/output/
├── cover.yuv                       # 封面 YUV420P（3072×4096）
├── cover.jpg                       # 封面 JPG（原图直出）
├── cover_exif.txt                  # 封面 EXIF：曝光时间/ISO/DateTimeOriginal
├── video/                          # live 流逐帧
│   ├── 001.yuv ... 077.yuv         # 逐帧 YUV420P（1728×1296）
│   └── 001.jpg ... 077.jpg         # 逐帧 JPG
└── frame_meta.txt                  # 逐帧 meta：index / pts_sec / exposure_time / iso / raw
```

## 8. 代码结构

| 文件 | 职责 |
|---|---|
| `inc/LivePhotoDecode/LpDecodeDefine.h` | 路径常量（input.jpg / mp4 / output 目录），函数内 static 返回 |
| `inc/LivePhotoDecode/LpDecode.h` | 编排入口 + 四个子步骤（封面、视频、EXIF、meta） |
| `inc/LivePhotoDecode/LpMetaParser.h` | mett packet 解析 + 字段锚定 |
| `src/LivePhotoDecode/LpMetaParser.cpp` | 上述实现（非 inline，避免头文件膨胀） |
| `tests/LivePhotoDecode/LivePhotoDecodeTest.h` | `RunLivePhotoDecodeTest()` 入口 |
| `tests/LivePhotoDecode/README.md` + `inc/LivePhotoDecode/README.md` | 文档 |

## 9. 留给实现者的自由

- 具体函数拆分与命名（遵循编码规范）。
- mett 解析的容差阈值、float 对齐策略。
- 是否把封面 YUV 生成拆成独立函数。

## 10. 验证

- 构建：`scripts\build-debug.bat camera_utils_tests`。
- 运行：`out\build\x64-debug\tests\camera_utils_tests.exe livephoto-decode`（或 `CameraUtils.exe livephoto-decode`）。
- 人工核对：`cover_exif.txt` 的曝光时间/ISO 与 `frame_meta.txt` 第 0 帧锚定值一致；`video/` 帧数与 `ffprobe nb_frames=77` 一致；`cover.jpg` 尺寸 3072×4096。
