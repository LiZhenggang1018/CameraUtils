// 各模块使用示例见/tests/路径下各测试用例

CustomLog - 可变参数日志打印
功能：简化可变参数日志打印，自动添加空格分隔。

```cpp
// 调用示例
PV("Hello", "world", 42, "!");
// 输出: Hello world 42 !
```



Singleton（CRTP单例接口）

ScopedUtils - 作用域工具
功能：自动记录函数入口/退出、耗时统计、资源释放(ON_SCOPE_EXIT作用域退出后自动清理宏等功能)。

```cpp
// 作用域计时器（自动计算函数执行时间）
{
SCOPED_TIMER();  // 打印: xxx.cpp: L123 funcName () begin
//       xxx.cpp: L123 funcName () end, cost time: 2000 us
std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

// 作用域日志（自动记录函数进出）
{
SCOPED_LOGGER(); // 打印: xxx.cpp: L123 funcName () enter
//       xxx.cpp: L123 funcName () exit
}

// RAII 资源释放（类似 Go 的 defer）
{
char* buf = (char*)malloc(512);
ON_SCOPE_EXIT {
free(buf);
buf = nullptr;
};
// 函数结束自动释放
}
```

CheckUtils - 校验工具
功能：常用校验宏和模板函数。

```cpp
// 条件校验，不满足则返回
CHECK_AND_RETURN(cond);           // 不满足条件则 return
CHECK_AND_RETURN_RET(cond, retval); // 不满足条件则 return retval

// 浮点数相等判断
bool equal = FEqual(1.5f, 1.5f);

// 字节序转换
uint32_t le = SwapEndian<uint32_t>(0x12345678);  // 大端转小端

// 字符串判断
bool hasPrefix = StartsWith(str, "prefix");
bool hasSuffix = EndsWith(str, "suffix");
```

Type2InstMap - 类型到实例映射
功能：将单例扩展为单例池，支持按类型获取不同实例。

```cpp
// 定义管理器映射
using ReqMgrMap = Type2InstMap<IVEReqType, RequestManager>;

// 注册实例
const std::vector<IVERReqType> types = {IVEReqType::CAP, IVEReqType::VID};
ReqMgrMap::Register(types);

// 使用
ReqMgrMap::GetMgr(IVEReqType::CAP).RemoveRequestData();

// 遍历所有实例
ReqMgrMap::ForEachMgr([&](auto& mgr) { mgr.MethodInA(); });
```

TimeoutMonitor - 超时监控
功能：异步执行函数并监控超时，超时执行回调。

````cpp
// 基本用法
auto timeoutCb = [](){ PV("Timeout happened!"); };
int result = TimeoutMonitor("MyFunction", 1000, myFunc, timeoutCb, arg1);

// 超时后直接终止进程（用于 HIDL IPC 场景）
TimeoutAbortMonitor("HidlCall", 3000, hidlFunc, arg1);
```
````

InstanceCounter - 实例计数器
功能：CRTP 模式实现实例计数，调试内存泄漏。

```cpp
class MyClass : public Counter<MyClass> {
public:
MyClass() = default;
virtual ~MyClass() = default;
};

// 使用
MyClass a, b, c;
int count = MyClass::GetInstanceCount();  // 返回 3
```

CircularQueue - 循环队列
功能：线程安全的环形缓冲区，支持覆盖模式。

```cpp
// 非阻塞循环队列
NonBlockingCircularQueue<int> queue(3);

queue.Push(1);
queue.Push(2);
queue.Push(3);

// 队列满时继续插入会覆盖最旧元素
queue.Push(4);  // 覆盖 1
queue.Push(5);  // 覆盖 2

// 队列内容: 3, 4, 5
```

DoubleEndedBlockingQueue - 双端阻塞队列
功能：支持从队首/队尾 Push/Pop，可配置阻塞/非阻塞。

```cpp
// 阻塞式双端队列
BlockingDeque<int> deque(5);
deque.Start();

// 生产者
deque.PushBack(1);    // 队尾插入
deque.PushFront(0);   // 队首插入

// 消费者
int val;
deque.PopFront(val);  // 从队首取出
deque.PopBack(val);   // 从队尾取出
```

事件系统 (Event)
概述
事件系统允许两个独立的 C++ 模块通过事件进行通信，发送者发送事件，接收者注册监听并接收事件。
实现单进程内跨线程、跨编译单元、极低时延、使用简单、替代metadata不能支撑的场景（如：跨RealtimePipe、后端发送到前端、无依赖注入关系的独立模块通信）的支持任意C++模块通信的事件订阅机制

┌─────────────────────────────────────────────────────────────┐
│                      事件系统架构                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐    ┌──────────────────┐    ┌────────────┐ │
│  │ EventSender │───▶│  EventDispatcher │───▶│EventReceiver│ │
│  └─────────────┘    │   (单例/线程池)   │    └────────────┘ │
│                     └──────────────────┘          │         │
│                                                    ▼         │
│                     ┌──────────────────┐    ┌────────────┐  │
│                     │   MsgQueue       │◀───│ 回调函数   │  │
│                     │  (异步执行)      │    └────────────┘  │
│                     └──────────────────┘                    │
└─────────────────────────────────────────────────────────────┘

消息队列/线程池 (ThreadPool)
概述
提供轻量级线程池实现，支持任务异步执行、同步等待。

```cpp
使用示例
ThreadPool queue(4);  // 创建 4 个工作线程

// 提交任务
auto future = queue.EnqueMsg([](int a, int b) {
return a + b;
}, 1, 2);

// 等待任务完成
queue.WaitSync(future);
int result = future.get();

// 等待所有任务完成
queue.WaitAll();
```

同步机制 (SyncAgency)
概述
提供跨模块的同步机制，支持一个模块等待另一个模块完成特定操作。

```cpp
使用示例
// 模块 A：通知完成
auto notifier = SyncAgency::GetInstance().Register(SyncId::CUSTOM_SIGNAL);
// 执行业务逻辑...
notifier->SetState(SyncState::DONE);

// 模块 B：等待完成
SyncAgency::GetInstance().WaitSync(SyncId::CUSTOM_SIGNAL, 5000);  // 等待 5 秒
```

Overload：防止隐式转换的Variant-Visitor静态多态模式的overload实现
