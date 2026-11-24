# clang-tidy 配置说明

本文档解释本工程 `.clang-tidy` 的检查范围与 CI 门禁策略。

## 一、启用的检查类别

`.clang-tidy` 的 `Checks` 采用「全开 + 精挑排除」策略：

```yaml
Checks: >
  -*,
  bugprone-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  clang-analyzer-*,
  ...（排除 8 项）
```

| 类别 | 检查内容 |
|---|---|
| `bugprone-*` | 易错模式：use-after-move、悬垂引用、错误内存操作、switch fallthrough、整型除法、无符号溢出等 |
| `cppcoreguidelines-*` | C++ 核心指南：范围边界、所有权、指针/引用安全、const 正确性、初始化顺序等 |
| `modernize-*` | 现代化写法：nullptr、auto、=default、std::move、emplace、pass-by-value 等 |
| `performance-*` | 性能：不必要拷贝、按值传参、for-range 拷贝、低效 vector 操作、noexcept-move 等 |
| `readability-*` | 可读性：命名规范、魔法数字、隐式转换、静态访问等 |
| `clang-analyzer-*` | 静态分析器：空指针解引用、内存泄漏、未初始化值、死代码等深度分析 |

### 排除的 8 项及原因

| 排除项 | 原因 |
|---|---|
| `-cppcoreguidelines-avoid-magic-numbers` | 魔法数字（书中示例大量用） |
| `-cppcoreguidelines-pro-bounds-pointer-arithmetic` | 指针算术（图像算法绕不开） |
| `-cppcoreguidelines-pro-type-reinterpret-cast` | reinterpret_cast（二进制处理需要） |
| `-modernize-use-trailing-return-type` | 尾置返回类型（风格不强制） |
| `-readability-magic-numbers` | 同 magic numbers |
| `-readability-identifier-length` | 标识符长度（不强制） |
| `-performance-avoid-endl` | std::endl（规范示例自己用，且 endl 有 flush 语义） |
| `-clang-analyzer-optin.performance.Padding` | 字节填充优化（HdrConfig 按功能分组） |

## 二、命名规范（CheckOptions 重点）

`.clang-tidy` 里配置最多的部分，通过 `readability-identifier-naming` 落地 OpenHarmony 命名规范：

| 对象 | 规则 |
|---|---|
| 类型（类/结构/枚举/联合/namespace/typedef） | 大驼峰 `CamelCase` |
| 函数 | 大驼峰 `CamelCase` |
| 变量 / 参数 | 小驼峰 `camelBack` |
| 私有/受保护成员 | 小驼峰 + `_` 后缀 |
| 公有成员 | 小驼峰（不加后缀，豁免 struct 聚合体） |
| 全局变量 | `g_` 前缀 + 小驼峰 |
| 全局/类常量、枚举值、宏 | 全大写 `UPPER_CASE` |
| 局部常量、普通 const 成员 | 小驼峰 `camelBack` |

### 豁免正则（IgnoredRegexp）

| 正则 | 豁免对象 |
|---|---|
| `.*_$` | 带下划线后缀（const 成员规则冲突） |
| `^__.*$` | 双下划线（编译器保留标识符） |
| `^(to_json\|from_json)$` | nlohmann json 的 ADL 钩子 |
| `^(begin\|end)$` | 容器迭代接口 |

## 三、其它关键配置

| 配置 | 值 | 作用 |
|---|---|---|
| `WarningsAsErrors` | `''`（空） | 警告不升级为错误（本地跑不阻断） |
| `HeaderFilterRegex` | `(src\|inc\|tests)[\\/]` | 只检查本工程头文件，排除 `_deps/`、`third_party/` |
| `FormatStyle` | `file` | 用 `.clang-format` |

## 四、CI 门禁分层（重要）

`tidy.yml`（CI）与本地 `.clang-tidy` 的门禁**不同**：

```powershell
clang-tidy ... --warnings-as-errors='clang-analyzer-*'
```

**只有 `clang-analyzer-*`（静态分析器）的告警会升级为错误、导致 CI 失败。**

| 类别 | CI 是否阻断 |
|---|---|
| `clang-analyzer-*` | ✅ **硬门禁**（`--warnings-as-errors`） |
| `bugprone-*` / `cppcoreguidelines-*` / `modernize-*` / `performance-*` / `readability-*` | ⚠️ 软门禁（只报告，不阻断） |

> 设计意图（`tidy.yml` 注释）：`bugprone and style checks are still triaged as warnings until
> the existing backlog is cleared` —— 先只对「静态分析器发现的真实缺陷」设硬门禁，
> 风格/性能类先报告不阻断，等存量告警清零后再收紧。

## 五、如何运行

```bat
:: 本地跑 clang-tidy（配合 x64-release 的 14.44 环境，见 AGENTS.md）
call "D:\Program Files\visual_studio\VC\Auxiliary\Build\vcvars64.bat"
"D:\Program Files\visual_studio\VC\Tools\Llvm\x64\bin\clang-tidy.exe" -p out/build/x64-release <文件>

:: CI 自动跑（.github/workflows/tidy.yml），硬门禁 clang-analyzer-*
```

## 六、与编译器警告的区别

clang-tidy 检查项与「编译器警告（`-W*`）」是**两套独立机制**：
- clang-tidy：静态分析工具，通过 `.clang-tidy` 配置，跑 `clang-tidy` 时检查
- 编译器警告：clang/MSVC 编译器的警告标志（`-W*` / `/W4`），编译时检查

两者的映射关系见 `docs/WARNING_MAPPING.md`。
