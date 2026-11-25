# inc/LivePhotoDecode —— LivePhoto 解码特性（接口层）

## 概述

解析 vivo X200S 拍摄的 LivePhoto（动态照片）：封面 JPG + 配套 MP4 视频。
产出封面与视频逐帧的 YUV/JPG、封面 EXIF 曝光信息、以及随帧 meta。

## 输入

| 文件 | 说明 |
| :--- | :--- |
| `data/LivePhotoDecode/input.jpg` | 封面照片（3072×4096，内嵌 EXIF） |
| `data/LivePhotoDecode/IMG_20261125_112849.mp4` | live 视频流（HEVC 1728×1296，77 帧，含 mett 轨道） |

## 输出（`data/LivePhotoDecode/output/`）

| 文件/目录 | 说明 |
| :--- | :--- |
| `cover.jpg` / `cover.yuv` | 封面 JPG 与 YUV420P |
| `cover_exif.txt` | 封面 EXIF：曝光时间 / ISO / DateTimeOriginal + 全部 EXIF 条目 |
| `video/{1..77}.yuv` / `{1..77}.jpg` | live 流逐帧 |
| `frame_meta.txt` | 逐帧 meta：帧号 / PTS 秒 / 曝光(推测) / ISO(推测) / 原始 float |

## 文件结构

| 文件 | 职责 |
| :--- | :--- |
| `LpDecodeDefine.h` | 路径常量（函数内 static 返回） |
| `LpDecode.h` | 编排接口：封面 / EXIF / live 流 |
| `LpMetaParser.h` | mett 随帧 meta 解析接口与数据结构 |

## mett 轨道逆向（重要边界）

vivo 的 `mett` 数据轨道（codec_tag `0x7474656d`）承载随帧 meta，结构为 77 个样本，
每个样本以 4 字节 tag `00 0F A0 0A` 开头 + 4 字节 big-endian 长度 + payload（嵌套 TLV + IEEE754 float）。

**字段语义未公开**，本模块只可靠输出：

- 每帧时间戳 `pts_seconds`（由 mett 流 time_base 1/90000 换算，确凿可得）；
- 每帧样本内解析出的原始 float 值（`raw=` 列，保留待后续逆向）。

封面 EXIF 的曝光时间（0.002s）与 ISO（1912）**并不以普通 float 出现在 mett 流中**
（实测全文件无 ISO 命中、0.002 仅两处无关偏移），故 `exposure_time` / `iso` 列输出 -1
并标注 `(speculative)`，表示「未能从 mett 流中锚定到对应字段」，而非确证缺失。
后续若发现 mett 里曝光信息以非线性/定点形式存储，可在此补充解码。
