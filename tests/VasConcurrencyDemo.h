#ifndef NEWTEMP_H
#define NEWTEMP_H

// 本demo基于C++20标准，使用了stdexec库（C++26std::execution标准库的前身）来实现异步任务的编排和执行。
// 用于模拟VAS中多帧处理的场景，展示如何创建多个Node对象，使用线程池并发执行任务，并保证回调任务的顺序执行（每帧异步处理的结果按照帧号顺序回给VCF）。

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#if defined(__has_include)
#if __has_include(<exec/static_thread_pool.hpp>) && __has_include(<stdexec/execution.hpp>)
#define VAS_HAS_STDEXEC 1
#else
#define VAS_HAS_STDEXEC 0
#endif
#else
#define VAS_HAS_STDEXEC 0
#endif

#if VAS_HAS_STDEXEC

struct NodeBase
{
    virtual int ProcessFrame(int) = 0; // ProcessFrame表示一帧yuv处理，
    // 入参int表示输入数据（真实case下对应输入yuvBuffer，来自上一个Node），返回int表示处理结果（真实case下对应输出yuvBuffer，给到下一个Node）
    // 真实case下：shared_ptr<ICameraBuffer> ProcessFrame(shared_ptr<ICameraBuffer> inYuv)
    virtual ~NodeBase() = default;

    // 模拟耗时操作，模拟真实yuv处理耗时,每次Node处理时间为随机的1s~3s
    void TimeConsuming()
    {
        thread_local std::mt19937 rng{std::random_device{}()};
        thread_local std::uniform_int_distribution<int> dist(1, 3); // 1 到 3 秒
        std::this_thread::sleep_for(std::chrono::seconds(dist(rng)));
    }
};

// 派生类 Node1，对应VAS中负责第一步处理的Node（第一个Algo）
struct Node1 : NodeBase
{
    int ProcessFrame(int x) override
    {
        TimeConsuming();
        auto t = x + 10 * x; // 11
        std::cout << "Node1::ProcessFrame result " << t << std::endl;
        return t;
    }
};

struct Node2 : NodeBase
{
    int ProcessFrame(int x) override
    {
        TimeConsuming();
        auto t = x + 100 * (x / 10); // 111
        std::cout << "Node2::ProcessFrame result " << t << std::endl;
        return t;
    }
};

struct Node3 : NodeBase
{
    int ProcessFrame(int x) override
    {
        TimeConsuming();
        auto t = x + 1000 * (x / 100); // 1111
        std::cout << "Node3::ProcessFrame result " << t << std::endl;
        return t;
    }
};

// CbFrameToVcf: 返回最终处理结果给VCF。在VAS中对应每帧处理完成后，结果回调给VCF的函数。
auto CbFrameToVcf(int value)
{
    return stdexec::just(value) | stdexec::then(
                                      [](int v)
                                      {
                                          std::printf("CbFrameToVcf received: %d\n", v);
                                          return v;
                                      });
}

inline void VasConcurrencyTest()
{
    auto main_start = std::chrono::steady_clock::now();
    // 1. 创建对象并存入 nodes 。对应VAS创建特定Pipeline，挂载各个对应Node（Node1->Node2->Node3）
    std::vector<std::shared_ptr<NodeBase>> nodes;
    nodes.push_back(std::make_shared<Node1>());
    nodes.push_back(std::make_shared<Node2>());
    nodes.push_back(std::make_shared<Node3>());
    // 2. 创建线程池和调度器
    exec::static_thread_pool pool(
        8); // 创建一个包含8个线程的线程池，exec::static_thread_pool是work_stealing线程池的一种实现。
    auto sched = pool.get_scheduler();
    // 辅助 lambda：根据初始值生成 processOneFrameTask sender（对应一次Node1→Node2→Node3的处理流）
    auto processOneFrameTask = [&](int init)
    {
        return stdexec::just(init) |
               stdexec::let_value(
                   [&nodes, sched](int val)
                   {
                       return stdexec::on(sched, stdexec::just(val)) |
                              stdexec::then([&nodes](int v) { return nodes[0]->ProcessFrame(v); });
                   }) |
               stdexec::let_value(
                   [&nodes, sched](int val)
                   {
                       return stdexec::on(sched, stdexec::just(val)) |
                              stdexec::then([&nodes](int v) { return nodes[1]->ProcessFrame(v); });
                   }) |
               stdexec::let_value(
                   [&nodes, sched](int val)
                   {
                       return stdexec::on(sched, stdexec::just(val)) |
                              stdexec::then([&nodes](int v) { return nodes[2]->ProcessFrame(v); });
                   });
    };

    // 3. 生成三个独立的 processFrame sender，对应处理3帧，每帧的初始值分别为
    // 1、2、3。对应VAS中同时有多帧在不同Node上处理（ Node1→Node2→Node3 ）的情况。
    auto processFrame_1 = processOneFrameTask(1);
    auto processFrame_2 = processOneFrameTask(2);
    auto processFrame_3 = processOneFrameTask(3);

    // ===== 并发启动三个帧处理 =====
    // 用 split 得到可共享结果的 sender，
    // split 将一次性异步操作转换为可共享的、可被多次等待的异步句柄，常见于“先启动后等待”或“多消费者等待同一结果”场景。
    auto split1 = stdexec::split(std::move(processFrame_1));
    auto split2 = stdexec::split(std::move(processFrame_2));
    auto split3 = stdexec::split(std::move(processFrame_3));

    // 拷贝一份用于启动，原始对象保留给回调链
    std::cout << "Starting frame processing tasks...\n";
    stdexec::start_detached(split1); // 线程池上立即开始执行帧1，前面的split（）保证了这里processFrame_1
                                     // 在被 start_detached 消费后，后面的回调仍然可以等待到结果。
    stdexec::start_detached(split2); // 帧2
    stdexec::start_detached(split3); // 帧3

    // ===== 构造顺序回调链 =====
    // 依次等待帧结果，并执行回调，回调之间严格顺序（CbFrame1 -> CbFrame2 ->CbFrame3）
    auto orderedCallbacks =
        stdexec::just()
        // mutable保证lambda的operator() 变为非
        // const，从而允许修改（包括移动）捕获的变量。否则默认捕获的变量在lambda内部是不可修改的（const），无法实现移动语义std::move(s1)。
        |
        stdexec::let_value(
            [s1 = std::move(split1)]() mutable
            {
                // 等待帧1结果，执行回调1
                return std::move(s1) | stdexec::let_value([](int r1) { return CbFrameToVcf(r1); });
            }) |
        stdexec::let_value(
            [s2 = std::move(split2)](auto) mutable
            {
                // 此时回调1已完成，再等待帧2结果，执行回调2
                return std::move(s2) | stdexec::let_value([](int r2) { return CbFrameToVcf(r2); });
            }) |
        stdexec::let_value(
            [s3 = std::move(split3)](auto) mutable
            {
                // 回调2已完成，再等待帧3结果，执行回调3
                return std::move(s3) | stdexec::let_value([](int r3) { return CbFrameToVcf(r3); });
            });

    // 同步等待整个回调链完成
    std::cout << "Waiting for ordered callbacks to complete...\n";
    auto result = stdexec::sync_wait(std::move(orderedCallbacks));
    if (result)
    {
        std::cout << "Final result after sync_wait: " << std::get<0>(*result) << std::endl;
    }

    auto main_end = std::chrono::steady_clock::now();
    auto main_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(main_end - main_start).count();
    std::cout << "main() execution time: " << main_duration << " ms" << std::endl;

    return;
}
#endif // VAS_HAS_STDEXEC
#endif // NEWTEMP_H
