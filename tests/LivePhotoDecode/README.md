# tests/LivePhotoDecode —— 测试说明

## 运行

```bat
out\build\x64-debug\tests\camera_utils_tests.exe livephoto-decode
```

或经 CTest（media 组）：

```bat
ctest --preset x64-debug-media
```

## 输入

- `data/LivePhotoDecode/input.jpg` —— 封面照片
- `data/LivePhotoDecode/IMG_20261125_112849.mp4` —— live 视频流

## 预期输出

| 产物 | 预期 |
| :--- | :--- |
| `output/cover.jpg` | 3072×4096 |
| `output/cover.yuv` | 大小 = 3072×4096×3/2 |
| `output/cover_exif.txt` | 曝光时间 0.002、ISO 1912、DateTimeOriginal 2026:11:25 11:28:49 |
| `output/video/` | 77 个 `.yuv` + 77 个 `.jpg`（= `ffprobe nb_frames`） |
| `output/frame_meta.txt` | 77 帧，每帧含 `pts_seconds` / `iso` / `timestamp_ns` / `gain_*` 列 |

## 归类

命令注册为 `Label::MEDIA`（依赖 `data/` 素材）。CI 无素材时按 `[SKIP] return 77` 跳过，
不会「假通过」。
