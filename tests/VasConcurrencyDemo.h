#ifndef NEWTEMP_H
#define NEWTEMP_H

// 本demo基于C++20标准，使用了stdexec库（C++26
// std::execution标准库的前身）来实现异步任务的编排和执行。
// 用于模拟VAS中多帧处理的场景，展示如何创建多个Node对象，使用线程池并发执行任务，并保证回调任务的顺序执行（每帧异步处理的结果按照帧号顺序回给VCF）。
// 对应.vsdx中 优化后的VAS页 描述的方案一

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

// constexpr std::chrono::seconds SLEEP_DURATION{ 1 }; // 1s睡眠时间，每个Node处理一帧的模拟耗时1s

// 基类 NodeBase
struct NodeBase
{
    virtual int ProcessFrame(int) = 0; // ProcessFrame表示一帧yuv处理，
    // 入参int表示输入数据（真实case下对应输入yuvBuffer，来自上一个Node），返回int表示处理结果（真实case下对应输出yuvBuffer，给到下一个Node）
    // 真实case下：shared_ptr<ICameraBuffer> ProcessFrame(shared_ptr<ICameraBuffer> inYuv)
    virtual ~NodeBase() = default;

    // 模拟耗时操作，模拟真实yuv处理耗时,每次Node处理时间为随机的1s~3s
    // void TimeConsuming() { std::this_thread::sleep_for(SLEEP_DURATION); }
    void TimeConsuming()
    {
        // 每个线程拥有独立的随机数生成器，避免加锁
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
        // std::cout << "Node1::ProcessFrame begin " << std::endl;
        TimeConsuming();
        auto t = x + 10 * x; // 11
        std::cout << "Node1::ProcessFrame result " << t << std::endl;
        return t;
    }
};

// 派生类 Node2
struct Node2 : NodeBase
{
    int ProcessFrame(int x) override
    {
        // std::cout << "Node2::ProcessFrame begin " << std::endl;
        TimeConsuming();
        auto t = x + 100 * (x / 10); // 111
        std::cout << "Node2::ProcessFrame result " << t << std::endl;
        return t;
    }
};

// 派生类 Node3
struct Node3 : NodeBase
{
    int ProcessFrame(int x) override
    {
        // std::cout << "Node3::ProcessFrame begin " << std::endl;
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

  