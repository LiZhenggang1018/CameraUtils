#ifndef THREADPOOLTEST_H
#define THREADPOOLTEST_H

#include "Utils.h"

class ThreadPoolTest
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
        ThreadPool pool(2);

        auto future1 = pool.EnqueMsg([]() { return 42; });
        auto future2 = pool.EnqueMsg([](int a, int b) { return a + b; }, 10, 20);

        pool.WaitSync(future1);
        pool.WaitSync(future2);

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
        ThreadPool pool(3);
        auto f = pool.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                PV("done");
                return;
            });
        pool.WaitSync(f);
        PV("TestCustom done");
    }

    static void TestWaitSync()
    {
        PV("TestWaitSync");
        ThreadPool pool(1);

        std::atomic<bool> taskCompleted{};
        auto future = pool.EnqueMsg(
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

        pool.WaitSync(future);

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
        ThreadPool pool(2);

        constexpr int taskCount = 10;
        std::vector<std::future<int>> futures;
        std::atomic<int> completedCount{0};

        for (int i = 0; i < taskCount; ++i)
        {
            futures.emplace_back(pool.EnqueMsg(
                [i, &completedCount]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    ++completedCount;
                    return i * i;
                }));
        }

        pool.WaitAll();

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
            ThreadPool pool(1);
            // 立即停止
        }

        ThreadPool pool(1);
        auto future = pool.EnqueMsg(
            []() -> int
            {
                PV("ok");
                return 42;
            });

        pool.WaitSync(future);
        int result = future.get();
        PV("Ok", result);
    }

    static void TestPerformance()
    {
        PV("TestPerformance");
        ThreadPool pool;

        constexpr int taskCount = 100;
        std::vector<std::future<int>> futures;
        futures.reserve(taskCount);

        auto startTime = std::chrono::steady_clock::now();

        for (int i = 0; i < taskCount; ++i)
        {
            futures.emplace_back(pool.EnqueMsg([i]() { return i * i; }));
        }

        pool.WaitAll();

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
        ThreadPool pool;
        // 并行处理一些计算任务
        auto future1 = pool.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                return "task1 done";
            });

        auto future2 = pool.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return 3.14159;
            });

        auto future3 = pool.EnqueMsg(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                return std::vector<int>{1, 2, 3, 4, 5};
            });

        // 等待特定任务
        pool.WaitSync(future2);
        PV("task2 done", future2.get());

        pool.WaitSync(future1);
        PV("task1 done", future1.get());

        pool.WaitSync(future3);
        auto vec = future3.get();
        PV("task3 done", vec.size());

        PV(pool.GetThreadCount(),
           "threads,",
           pool.GetPendingTaskCount(),
           "pending tasks,",
           pool.GetActiveTaskCount(),
           "active tasks");
    }
};

// 示例用法
inline void ThreadPoolTest()
{
    ThreadPoolTest::RunAllTests();
    return ;
}

#endif // !THREADPOOLTEST_H
