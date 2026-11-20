# inc/CommonUtils —— 公共工具类（头文件层）

## 概述

本目录存放**跨章节复用的公共工具类头文件**，与具体图像算法（第 3/7/8/9/10/11/12 章）无关。
这些头文件此前散落在 `inc/` 根目录，现统一归档于此，便于区分「工具层」与「算法层」。

`CoreUtils.h` 作为纯标准库聚合头，保留在 `inc/` 根目录（它汇总本目录下的大部分纯 C++ 工具）。

## 文件清单

| 头文件 | 用途 |
|---|---|
| `AllUsedHeadFiles.h` | 标准库头文件聚合（无第三方依赖） |
| `CheckUtils.h` | 断言/检查工具 |
| `CircularQueue.h` | 环形队列（阻塞/非阻塞） |
| `CommonEnv.h` | 环境路径（`EnvRootPath()`） |
| `ConcatCppFilesInDir.h` | 目录 cpp 拼接工具 |
| `CustomLog.h` | 自定义日志 |
| `DoubleEndedBlockingQueue.h` | 双端阻塞队列 |
| `EnumStrTraits.h` | 枚举↔字符串转换 |
| `Event.h` | 事件基础类型 |
| `EventComponents.h` | 事件组件 |
| `EventDispatcher.h` | 事件分发器 |
| `Exiv2Common.h` | exiv2 元数据封装 |
| `FFMpegCommon.h` | FFmpeg 封装 |
| `FilesUtils.h` | 文件系统工具 |
| `FindAndThenOrElse.h` | Find/AndThen/OrElse 组合 |
| `GeneralFactory.h` | 通用工厂模式 |
| `InstanceCounter.h` | 实例计数 |
| `JsonUtils.h` | JSON 序列化/反序列化（nlohmann） |
| `MagicEnumCommon.h` | magic_enum 封装 |
| `NoCopyableMacro.h` | 禁止拷贝宏 |
| `OpenCVCommon.h` | OpenCV 头文件聚合 |
| `Overload.h` | Overload 模式（variant 访问） |
| `ScopedUtils.h` | 作用域工具（timer/logger/exit） |
| `Singleton.h` | 单例基类 |
| `SplitCppFilesInDir.h` | 目录 cpp 拆分工具 |
| `SyncAgency.h` | 同步协调 |
| `ThreadPool.h` | 线程池 |
| `TimeoutMonitor.h` | 超时监控 |
| `Type2InstMap.h` | 类型→实例映射 |

## 定位

- 本目录头文件是**通用工具**，不依赖具体图像算法（OpenCV/FFmpeg/exiv2 封装头如
  `OpenCVCommon.h`、`FFMpegCommon.h`、`Exiv2Common.h` 除外，它们是第三方库的公共封装）。
- 算法相关代码请放对应章节目录（`inc/ImageProcessing/`、`inc/FeatureMatching/` 等）。
- 测试用例对应放在 `tests/CommonUtils/`。
