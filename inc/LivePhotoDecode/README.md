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
| `frame_meta.txt` | 逐帧 meta：帧号 / PTS 秒 / ISO / 纳秒时间戳 / 增益 |

## 文件结构

| 文件 | 职责 |
| :--- | :--- |
| `LpDecodeDefine.h` | 路径常量（函数内 static 返回） |
| `LpDecode.h` | 编排接口：封面 / EXIF / live 流 |
| `LpMetaParser.h` | mett 随帧 meta 解析接口与数据结构 |

## mett 轨道逆向（已确证字段）

vivo 的 `mett` 数据轨道（codec_tag `0x7474656d`）承载随帧 meta，结构为 1 个 `SAT` 头样本
+ N 个数据样本，每个样本以 4 字节 tag `00 0F A0 0A` 开头 + 4 字节 big-endian 长度 + payload。

经两次真实拍摄 + 毫秒秒表校准，已确证的逐帧字段（偏移相对样本起点）：

| 偏移 | 类型 | 含义 |
| :--- | :--- | :--- |
| `+36` | uint32 | 纳秒时间戳（帧间隔 33 333 300 ns = 1/30 s） |
| `+42` | uint16 | ISO |
| `+60` | float | 模拟增益 = ISO / 50（50 = 传感器 base ISO） |
| `+64` | float | 总增益 = 模拟增益 × 数字增益 |
| `+68` | float | 增益的冗余表达 |

确证依据（APEX 交叉验证，两次拍摄数据对齐）：

- `+60 / ISO == 0.02`（= 1/50）在 ISO 251 / 382 / 1912 三处严格成立；
- `+64 / +60 == 数字增益`（本次 1.0，另一次 1.0397）；
- 秒表校准确认封面照片 = 视频第 51 帧（误差 5 ms 内）。

快门速度不在上述字段中（两次拍摄快门均恒定，未暴露其字段位置）。
