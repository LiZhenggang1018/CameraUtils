# LinearFilterConfig.json 说明

线性滤波调参 demo 的配置文件，由 `LinearFilterConfig`（`inc/ImageProcessing/LinearFilterConfig.h`）序列化。

## 字段

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `filterType` | string | `"gaussian"` | 滤波类型：`gaussian` / `mean` / `sharpen` / `sobel` |
| `kernelSize` | int | `3` | 核尺寸（模糊类，正奇数） |
| `sigma` | double | `1.0` | 高斯标准差（≤0 时由 OpenCV 自动推导） |
| `sharpenAmount` | double | `1.0` | 锐化强度（unsharp 掩膜系数） |

## 说明

- 首次运行 `linear-filter-demo` 时若文件不存在，会自动生成默认值。
- 退出 demo 时会回写当前参数。
- 修改后重新运行 demo 即可生效。
