# clang 警告 → MSVC 等价物映射表

本项目日常用 **MSVC（cl.exe）** 编译，但很多资料/习惯用 clang 的 `-W*` 警告标志。本文档记录两者的等价关系，
以及在本项目的落地方式。

## 一、编译器警告（`-W*`）映射

| clang 标志 | 查什么 | MSVC 等价物 | 落地状态 |
|---|---|---|---|
| `-Wall` | 常用警告集合 | `/W4` | ✅ `x64-warnings` preset |
| `-Wswitch` | switch 缺 case | `/w14062`（C4062） | ✅ `x64-warnings` |
| `-Wswitch-enum` | switch 覆盖所有枚举值 | 无（MSVC 无严格等价） | ❌ 需 clang-cl |
| `-Wrange-loop-construct` | range-for 不必要拷贝 | 无（靠 clang-tidy `performance-for-range-copy`） | ✅ clang-tidy 已覆盖 |
| `-Wpessimizing-move` | 阻止优化的 std::move | 无（靠 clang-tidy `performance-*`） | ✅ clang-tidy 已覆盖 |
| `-Wunused-variable` | 未使用变量 | `/w14189`（C4189） | ✅ `x64-warnings` |
| `-Wunused-parameter` | 未使用形参 | `/w14100`（C4100） | ✅ `x64-warnings` |
| `-Wunused-private-field` | 未使用私有字段 | 无（MSVC 不查） | ❌ 需 clang-cl |
| `-Wreorder-ctor` | 初始化列表顺序不一致 | `/w15038`（C5038） | ✅ `x64-warnings` |
| `-Wreturn-type` | 缺返回值 | `/w14715`（C4715） | ✅ `x64-warnings` |

## 二、安全插桩映射

| clang 标志 | 作用 | MSVC 等价物 | 状态 |
|---|---|---|---|
| `-fstack-protector` | 栈保护（金丝雀） | `/GS` | ✅ Debug/Release 默认开启 |

## 三、MSVC 警告号语义（本项目用到的）

| 警告号 | 语义 |
|---|---|
| C4061 | switch 中 case 标签后无语句（枚举值被「跳过」，即 fallthrough） |
| C4062 | switch 中枚举值完全未被任何 case 覆盖 |
| C4100 | 未引用的形参 |
| C4189 | 局部变量已初始化但未被引用 |
| C5038 | 成员初始化顺序与声明顺序不一致 |
| C4715 | 不是所有控制路径都返回值 |

## 四、MSVC `/w1` 语法说明

`/w1<编号>` = 把警告 `<编号>` 设为 level 1（开启为普通警告）。

- `/W4`：启用所有 level 1~4 警告
- `/w14062`：显式开启 C4062（某些警告默认关闭，需 `/w1` 强制开）

## 五、无法用 MSVC 实现、需 clang-cl 的项

以下 3 项是 MSVC 编译器的能力限制，**路线 B（不装 clang）无法实现**：

1. `-Wswitch-enum`（严格语义：强制枚举所有值都有 case）
2. `-Wunused-private-field`（未使用的私有字段）
3. `-Wpessimizing-move` 的「编译器级」检测（注：clang-tidy 已覆盖此语义，见下）

> 注：`-Wrange-loop-construct` 和 `-Wpessimizing-move` 虽无法走 MSVC 编译器警告，但**已被 clang-tidy
> （`performance-for-range-copy`、`performance-*`）覆盖**，见 `.clang-tidy`，所以实际上不缺检查能力。

## 六、如何启用

```bat
:: 严格告警构建（/W4 + 上述显式警告）
cmake --preset x64-warnings
cmake --build --preset x64-warnings

:: 栈保护（/GS）已在 Debug/Release 默认开启，无需额外操作
```

## 七、clang-tidy 检查项（另一类，勿混淆）

本工程 `.clang-tidy` 已启用 `performance-*`、`modernize-*` 等，涵盖：
- `performance-unnecessary-value-param` / `unnecessary-copy-initialization` / `for-range-copy`
  / `inefficient-vector-operation` / `noexcept-move-constructor`
- `modernize-pass-by-value` / `modernize-use-emplace`

这些是 **clang-tidy check**，跑 `clang-tidy` 时检查，与「编译器警告」是两套独立机制。
