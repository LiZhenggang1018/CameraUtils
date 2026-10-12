#ifndef TESTCASE_H
#define TESTCASE_H

#include "CommonHead.h"

#include "EventTest.h"
#include "FactoryAndJson.h"
#include "FactoryDemo.h"
#include "FactoryDemo_1.h"
#include "SyncAgencyTest.h"

// All Testcases

// 测试BlockingDeque阻塞模式
inline void TestInBlockingMode()
{
    BlockingDeque<int> blockingDeque(5, 1000); // 阻塞模式,最大容量为5，超时为1000ms；

    // 启动队列操作
    blockingDeque.Start();

    // 生产者线程（阻塞队列）
    std::thread producerBlocking(
        [&blockingDeque]()
        {
            for (int i = 0; i < 10; ++i)
            {
                while (!blockingDeque.PushBack(i))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                std::cout << "Pushed: " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::cout << "Push stopped: " << std::endl;
        });

    // 消费者线程（阻塞队列）
    std::thread consumerBlocking(
        [&blockingDeque]()
        {
            int value;
            while (blockingDeque.PopFront(value))
            {
                std::cout << "  Popped: " << value << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
            std::cout << "  Pop stopped." << std::endl;
        });

    producerBlocking.join();
    consumerBlocking.join();

    blockingDeque.Stop();
    std::cout << "       Stop." << std::endl;
    return;
}

// 测试BlockingDeque非阻塞模式
inline void TestInNonBlockingMode()
{
    NonBlockingDeque<int> nonBlockingDeque(5); // 最大容量为5，非阻塞模式

    // 启动队列操作
    nonBlockingDeque.Start();

    // 生产者线程（非阻塞队列）
    std::thread producerNonBlocking(
        [&nonBlockingDeque]()
        {
            for (int i = 0; i < 10; ++i)
            {
                if (!nonBlockingDeque.PushBack(i))
                {
                    std::cout << "Push failed for value: " << i << std::endl;
                }
                else
                {
                    std::cout << "Pushed: " << i << std::endl;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::cout << "Pushed Stop " << std::endl;
        });

    // 消费者线程（非阻塞队列）
    std::thread consumerNonBlocking(
        [&nonBlockingDeque]()
        {
            int value;
            while (nonBlockingDeque.PopFront(value))
            {
                std::cout << "  Popped: " << value << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            std::cout << "  Pop stopped." << std::endl;
        });

    producerNonBlocking.join();
    consumerNonBlocking.join();

    nonBlockingDeque.Stop();
    std::cout << "              Stop" << std::endl;

    return;
}

inline void TestFindAndThenOrElse()
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    Find(vec, 3)
        .AndThen([](int value) { std::cout << "Found: " << value << std::endl; })
        .OrElse([]() { std::cout << "Not found" << std::endl; });
    Find(vec, 6)
        .AndThen([](int value) { std::cout << "Found: " << value << std::endl; })
        .OrElse([]() { std::cout << "Not found" << std::endl; });
}

// test code for OverloadNoCvt
struct B1
{
    int m_i{1};
};

struct B2
{
    float m_f{};
};

using C1 = std::variant<B1, B2>;

inline void OverloadRef(const C1& c)
{
    std::visit(
        OverloadNoCvt{[](auto arg) { PV(0); },
                      //[](B1& arg) { PV(1); return; },   //不行！不会走入此分支，静态断言会失效。
                      [](const B1& arg)
                      {
                          PV(1.5);
                          return;
                      }, // 走入此分支
                      //[](B1 arg) { PV(1.2); return; },   //同样可以走入此分支，但是，如果const B1&
                      //和 B1 两种形参的lambda都在的话，会重载失败
                      [](B2 arg) { PV(2); }},
        c);
    PV("aa");
}

inline void TestOverloadRef()
{
    C1 c1 = B1{10};
    OverloadRef(c1);
}

struct TimeoutTestCase
{
public:
    // 使用示例：
    int Foo(const float& b)
    {
        auto cb = [this](const float& b) { return ToBeMonitoredFunc(b); };
        // return TimeoutAbortMonitor("ToBeMonitoredFunc", 3000, cb, b);   // 1000/3000 to test
        // timeout or not
        auto timeoutCb = []() { PV("Timeout happened!"); };
        return TimeoutMonitor(
            "ToBeMonitoredFunc", 1000, cb, timeoutCb, b); // 1000/3000 to test timeout or not
    }

    // 这是一个耗时的成员函数，需要监控其超时
    int ToBeMonitoredFunc(const float& b)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        return static_cast<int>(b);
    }
};

inline void TimeoutMonitorTest()
{
    TimeoutTestCase testCase{};
    int ret = testCase.Foo(3.14f);
    return;
}

enum Color
{
    Red = 1,
    Green = 2,
    Blue = 3
};

inline void MiscTest()
{
    PV(1, "a");
    CAR(1 == 1);

    PV(Enum2Str<Color>(Red)); // 打印Red
    PV(Enum2Str<Color>(1));   // 同样打印Red
    std::string s("Red");
    if (auto e =
            Str2Enum<Color>(s)) // e是 optional<EN>类型,当一个 optional<T> 对象被按语境转换成 bool
                                // 时，若对象含值则转换返回 true ，若对象不含值则返回 false 。
    {
        PV(*e); // 打印1;
    }

    SCOPED_TIMER();
    SCOPED_LOGGER();
    ON_SCOPE_EXIT
    {
        PV("exit scope");
    };

    InstanceCounterTest();
    return;
}

inline void TestCircularQueue()
{
    // BlockingCircularQueue<int> queue(3); // Test BlockingCircularQueue
    NonBlockingCircularQueue<int> queue(3); // Test NonBlockingCircularQueue

    std::thread producer(
        [&queue]()
        {
            for (int i = 0; i < 5; ++i)
            {
                queue.Push(i);
                PV("Pushed:", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

    std::thread consumer(
        [&queue]()
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            int value;
            for (int i = 0; i < 5; ++i)
            {
                // 对于非阻塞式的队列为空时的Pop()，这里会返回失败，可以考虑添加返回值判断
                // 对于阻塞式的队列为空时的Pop()，这里会一直等待直到有数据
                queue.Pop(value);
                PV("Popped:", value);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });

    producer.join();
    consumer.join();
}

// 测试覆盖功能的函数
inline void TestOverwriteFunction()
{
    PV("=== Testing Overwrite Function ===:");
    BlockingCircularQueue<int> queue(3);

    // 正常插入3个元素
    queue.Push(1);
    queue.Push(2);
    queue.Push(3);
    PV("After pushing 1,2,3 - Size:", queue.Size());

    // 继续插入，应该覆盖最旧的元素
    queue.Push(4); // 应该覆盖1
    queue.Push(5); // 应该覆盖2
    PV("After pushing 4,5 - Size:", queue.Size());

    int value;
    while (!queue.Empty() && queue.Pop(value))
    {
        PV("Popped:", value);
    }
    // 期望输出: 3, 4, 5
}

// 测试代码
class MsgQueueTest
{
public:
    static void RunAllTests()
    {
        PV("RunAllTests beg");
        TestCustom();
        TestBasicFunctionality();
        TestWaitSync();
        TestWaitAll();
        TestErrorHandling();
        TestPerformance();
        TestCustomFunctionality();

        PV("RunAllTests end");
    }

private:
    static void TestBasicFunctionality()
    {
        PV("TestBasicFunctionality");
        ThreadPool msgQueue(2);

        auto future1 = msgQueue.EnqueMsg([]() { return 42; });
        auto future2 = msgQueue.EnqueMsg([](int a, int b) { return a + b; }, 10, 20);

        msgQueue.WaitSync(future1);
        msgQueue.WaitSync(future2);

        int result1 = future1.get();
        int result2 = future2.get();

        if (result1 == 42)
        {
            PV("t1 ok");
        }
        else
        {
            PV("t1 err");
        }

        if (result2 == 30)
        {
            PV("t2 ok");
        }
        else
        {
            PV("t2 err");
        }
    }

    static void TestCustom()
    {
        PV("TestCustom");
        ThreadPool msgQueue(3);
        auto f = msgQueue.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                PV("done");
                return;
            });
        msgQueue.WaitSync(f);
        PV("TestCustom done");
    }

    static void TestWaitSync()
    {
        PV("TestWaitSync");
        ThreadPool msgQueue(1);

        std::atomic<bool> taskCompleted{};
        auto future = msgQueue.EnqueMsg(
            [&taskCompleted]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                taskCompleted = true;
                return "completed";
            });

        if (!taskCompleted)
        {
            PV("task not begin yet");
        }

        msgQueue.WaitSync(future);

        if (taskCompleted)
        {
            PV("taskCompleted after WaitSync");
        }
        else
        {
            PV("not taskCompleted after WaitSync");
        }

        std::string result = future.get();
        PV(result);
    }

    static void TestWaitAll()
    {
        PV("TestWaitAll");
        ThreadPool msgQueue(2);

        constexpr int taskCount = 10;
        std::vector<std::future<int>> futures;
        std::atomic<int> completedCount{0};

        for (int i = 0; i < taskCount; ++i)
        {
            futures.emplace_back(msgQueue.EnqueMsg(
                [i, &completedCount]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    ++completedCount;
                    return i * i;
                }));
        }

        msgQueue.WaitAll();

        if (completedCount == taskCount)
        {
            PV(taskCount, "tasks done");
        }
        else
        {
            PV("err,completed tasks is expected", taskCount, ", but only", completedCount);
        }

        bool allCorrect = true;
        for (int i = 0; i < taskCount; ++i)
        {
            int result = futures[i].get();
            if (result != i * i)
            {
                PV("err at", i);
                allCorrect = false;
            }
        }

        if (allCorrect)
        {
            PV("All pass");
        }
    }

    static void TestErrorHandling()
    {
        PV("TestErrorHandling");

        // 测试已停止的线程池
        {
            ThreadPool msgQueue(1);
            // 立即停止
        }

        ThreadPool msgQueue(1);
        auto future = msgQueue.EnqueMsg(
            []() -> int
            {
                PV("ok");
                return 42;
            });

        msgQueue.WaitSync(future);
        int result = future.get();
        PV("Ok", result);
    }

    static void TestPerformance()
    {
        PV("TestPerformance");
        ThreadPool msgQueue;

        constexpr int taskCount = 100;
        std::vector<std::future<int>> futures;
        futures.reserve(taskCount);

        auto startTime = std::chrono::steady_clock::now();

        for (int i = 0; i < taskCount; ++i)
        {
            futures.emplace_back(msgQueue.EnqueMsg([i]() { return i * i; }));
        }

        msgQueue.WaitAll();

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // 验证所有结果正确
        bool allCorrect = true;
        for (int i = 0; i < taskCount; ++i)
        {
            int result = futures[i].get();
            if (result != i * i)
            {
                allCorrect = false;
                break;
            }
        }

        if (allCorrect)
        {
            PV("ok, done", taskCount, "tasks at", duration.count(), "ms");
        }
        else
        {
            PV("err");
        }
    }

    static void TestCustomFunctionality()
    {
        PV("TestCustomFunctionality");
        // 在此处添加自定义测试逻辑
        ThreadPool msgQueue;
        // 并行处理一些计算任务
        auto future1 = msgQueue.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                return "task1 done";
            });

        auto future2 = msgQueue.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return 3.14159;
            });

        auto future3 = msgQueue.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                return std::vector<int>{1, 2, 3, 4, 5};
            });

        // 等待特定任务
        msgQueue.WaitSync(future2);
        PV("task2 done", future2.get());

        msgQueue.WaitSync(future1);
        PV("task1 done", future1.get());

        msgQueue.WaitSync(future3);
        auto vec = future3.get();
        PV("task3 done", vec.size());

        PV(msgQueue.GetThreadCount(),
           "threads,",
           msgQueue.GetPendingTaskCount(),
           "pending tasks,",
           msgQueue.GetActiveTaskCount(),
           "active tasks");
    }
};

// 示例用法
inline void MsgQueueTest()
{
    MsgQueueTest::RunAllTests();
    return;
}

inline void RunAllTests()
{
    TestInBlockingMode();
    TestInNonBlockingMode();
    TestFindAndThenOrElse();
    TestOverloadRef();
    TimeoutMonitorTest();
    MiscTest();
    MsgQueueTest();
    TestCircularQueue();
    TestOverwriteFunction();
    TestEvent();
    FactoryJsonTest();
    TestSyncAgency();

    return;
}
#endif // !TESTCASE_H
