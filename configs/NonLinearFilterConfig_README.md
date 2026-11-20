# NonLinearFilterConfig.json 说明

非线性滤波调参 demo 的配置文件，由 `NonLinearFilterConfig`（`inc/ImageProcessing/NonLinearFilterConfig.h`）序列化。

## 字段

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `filterType` | string | `"median"` | 滤波类型：`median` / `bilateral` |
| `kernelSize` | int | `3` | 中值滤波核尺寸（正奇数） |
| `diameter` | int | `9` | 双边滤波空间核直径 |
| `sigmaColor` | double | `50.0` | 双边灰度相似度标准差 |
| `sigmaSpace` | double | `50.0` | 双边空间标准差 |

## 说明

- 首次运行 `nonlinear-filter-demo` 时若文件不存在，会自动生成默认值。
- 退出 demo 时会回写当前参数。
- 修改后重新运行 demo 即可生效。
