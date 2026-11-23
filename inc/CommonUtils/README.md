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

## 使用示例

### CustomLog

可变参数日志打印，自动添加空格分隔。

```cpp
PV("Hello", "world", 42, "!");
// 输出: Hello world 42 !
```

### Singleton

CRTP 单例接口。

### ScopedUtils

作用域工具：自动记录函数入口/退出、耗时统计、RAII 资源释放。

```cpp
{
    SCOPED_TIMER();   // 自动计算函数执行时间
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

{
    SCOPED_LOGGER();  // 自动记录函数进出
}

{
    char* buf = (char*)malloc(512);
    ON_SCOPE_EXIT {
        free(buf);
        buf = nullptr;
    };
}
```

### CheckUtils

常用校验宏和模板函数。

```cpp
CHECK_AND_RETURN(cond);               // 不满足条件则 return
CHECK_AND_RETURN_RET(cond, retval);   // 不满足条件则 return retval
bool equal = FEqual(1.5f, 1.5f);
uint32_t le = SwapEndian<uint32_t>(0x12345678);
bool hasPrefix = StartsWith(str, "prefix");
bool hasSuffix = EndsWith(str, "suffix");
```

### Type2InstMap

类型到实例映射（单例池）。

```cpp
using ReqMgrMap = Type2InstMap<IVEReqType, RequestManager>;
ReqMgrMap::Register({IVEReqType::CAP, IVEReqType::VID});
ReqMgrMap::GetMgr(IVEReqType::CAP).RemoveRequestData();
ReqMgrMap::ForEachMgr([](auto& mgr) { mgr.MethodInA(); });
```

### TimeoutMonitor

异步执行函数并监控超时，超时执行回调。

```cpp
auto timeoutCb = []() { PV("Timeout happened!"); };
int result = TimeoutMonitor("MyFunction", 1000, myFunc, timeoutCb, arg1);
TimeoutAbortMonitor("HidlCall", 3000, hidlFunc, arg1);
```

### InstanceCounter

CRTP 模式实现实例计数，调试内存泄漏。

```cpp
class MyClass : public Counter<MyClass> {
public:
    MyClass() = default;
    virtual ~MyClass() = default;
};
MyClass a, b, c;
int count = MyClass::GetInstanceCount();  // 3
```

### CircularQueue

线程安全的环形缓冲区，支持覆盖模式。

```cpp
NonBlockingCircularQueue<int> queue(3);
queue.Push(1); queue.Push(2); queue.Push(3);
queue.Push(4);  // 覆盖 1
```

### DoubleEndedBlockingQueue

双端阻塞队列，支持队首/队尾 Push/Pop。

```cpp
BlockingDeque<int> deque(5);
deque.Start();
deque.PushBack(1);
deque.PushFront(0);
int val;
deque.PopFront(val);
```

### Event

事件系统：单进程内跨线程、跨编译单元的模块间通信。

```
EventSender → EventDispatcher → EventReceiver
                (单例/线程池)
```

### ThreadPool

轻量级线程池，支持任务异步执行、同步等待。

```cpp
ThreadPool queue(4);
auto future = queue.EnqueMsg([](int a, int b) { return a + b; }, 1, 2);
queue.WaitSync(future);
int result = future.get();
```

### SyncAgency

跨模块同步机制。

```cpp
auto notifier = SyncAgency::GetInstance().Register(SyncId::CUSTOM_SIGNAL);
notifier->SetState(SyncState::DONE);
SyncAgency::GetInstance().WaitSync(SyncId::CUSTOM_SIGNAL, 5000);
```

### Overload

防止隐式转换的 Variant-Visitor 静态多态模式的 overload 实现。
