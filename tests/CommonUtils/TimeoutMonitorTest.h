#ifndef TESTS_TIMEOUTMONITORTEST_H
#define TESTS_TIMEOUTMONITORTEST_H

#include <atomic>
#include <memory>

#include "CoreUtils.h"
#include "cli/TestAssert.h"

// TimeoutImpl 参数传递语义测试。
//
// 覆盖四种参数形态，验证「值捕获（默认）」「std::ref 引用语义」「std::move 移动语义」：
//   1. void Foo(std::shared_ptr<int> sp)        —— by-value，替换句柄不影响原对象
//   2. void Foo(std::shared_ptr<int>& spref)    —— 引用，需 std::ref，替换句柄影响原对象
//   3. void Foo(std::unique_ptr<int> up)        —— by-value，需 std::move，所有权转移
//   4. void Foo(int& i)                          —— 引用，需 std::ref，修改原变量
//
// 均为「不超时」路径（sleep 远小于 timeMs），保证断言确定性；单次调用即可返回，
// 无 detach 后台残留，测试快速稳定。

namespace {

constexpr int kFastSleepMs = 10; // 被监控函数的执行时间
constexpr int kTimeoutMs = 500;  // 超时阈值（远大于 sleep，保证不超时）

constexpr int kSlowSleepMs = 200;   // 超时用例：被监控函数执行时间（大于阈值）
constexpr int kShortTimeoutMs = 50; // 超时用例：阈值（小于 sleep，触发超时）

// 超时用例的共享状态：detach 后测试函数返回，但后台线程仍在跑，
// 被引用对象必须是全局/static，生命周期覆盖后台线程，避免悬垂。
static std::atomic<int> g_timeoutHit{0}; // timeoutCb 被触发的次数
static int g_timeoutVal{0};              // 后台任务最终写入的值（FooSlow(int&) 写它）

// 1. by-value shared_ptr：替换局部副本，不影响调用者持有的原 shared_ptr
void FooByValueSharedPtr(std::shared_ptr<int> sp)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(kFastSleepMs));
    sp = std::make_shared<int>(11); // 替换副本的指向，原 sp 不受影响
}

// 2. shared_ptr&：替换引用指向的对象，影响调用者的原 shared_ptr
void FooByRefSharedPtr(std::shared_ptr<int>& spref)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(kFastSleepMs));
    spref = std::make_shared<int>(22); // 替换原 sp 的指向
}

// 3. by-value unique_ptr：所有权移入，后台操作其指向对象，调用者的 up 置空
void FooByValueUniquePtr(std::unique_ptr<int> up)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(kFastSleepMs));
    *up = 33; // 操作已转移所有权的对象
}

// 4. int&：修改原变量
void FooByRefInt(int& i)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(kFastSleepMs));
    i = 44;
}

// 超时用例：慢函数，sleep 大于阈值，触发超时
void FooSlow(int& i)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(kSlowSleepMs));
    i = 88;
}

} // namespace

inline void TimeoutMonitorArgTest()
{
    // 1. by-value shared_ptr：原 sp 指向不变（值捕获拷贝了 shared_ptr，Foo 替换的是副本）
    {
        auto sp = std::make_shared<int>(0);
        TimeoutMonitor("FooByValueSharedPtr", kTimeoutMs, FooByValueSharedPtr, [] {}, sp);
        CU_CHECK_EQ(*sp, 0); // 仍指向原对象，未被替换成 11
    }

    // 2. shared_ptr&：std::ref 保留引用语义，原 sp 被替换
    {
        auto sp = std::make_shared<int>(0);
        TimeoutMonitor("FooByRefSharedPtr", kTimeoutMs, FooByRefSharedPtr, [] {}, std::ref(sp));
        CU_CHECK_EQ(*sp, 22); // 原 sp 被替换指向 22
    }

    // 3. by-value unique_ptr：std::move 转移所有权，调用者的 up 置空
    {
        auto up = std::make_unique<int>(0);
        TimeoutMonitor("FooByValueUniquePtr", kTimeoutMs, FooByValueUniquePtr, [] {}, std::move(up));
        CU_CHECK(up == nullptr); // 所有权已移入后台，原 up 为空
    }

    // 4. int&：std::ref 保留引用语义，修改原变量
    {
        int i = 0;
        TimeoutMonitor("FooByRefInt", kTimeoutMs, FooByRefInt, [] {}, std::ref(i));
        CU_CHECK_EQ(i, 44); // 原变量被改为 44
    }
}

inline void TimeoutMonitorTimeoutTest()
{
    // 超时路径：被监控函数 sleep 200ms，阈值 50ms，验证：
    //   1. timeoutCb 被触发
    //   2. TimeoutMonitor 及时返回（耗时远小于 200ms，不阻塞到后台结束）
    //   3. detach 后台仍安全完成（改的是全局对象，不悬垂）
    g_timeoutHit = 0;
    g_timeoutVal = 0;

    // timeoutCb：超时后同步触发，计数 +1
    auto cb = [] { ++g_timeoutHit; };

    auto t0 = std::chrono::steady_clock::now();
    // 传 std::ref(g_timeoutVal)：引用全局 int，detach 后后台仍安全写它
    TimeoutMonitor("FooSlow", kShortTimeoutMs, FooSlow, cb, std::ref(g_timeoutVal));
    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    CU_CHECK_EQ(static_cast<int>(g_timeoutHit.load()), 1); // timeoutCb 恰被触发一次
    // 及时返回：耗时应小于 kSlowSleepMs（未阻塞到后台 200ms 结束）
    CU_CHECK(elapsed < kSlowSleepMs);

    // 等后台真正跑完，验证 detach 后引用语义仍正确（改的是全局对象）
    std::this_thread::sleep_for(std::chrono::milliseconds(kSlowSleepMs + 100));
    CU_CHECK_EQ(g_timeoutVal, 88);
}

#endif // TESTS_TIMEOUTMONITORTEST_H
