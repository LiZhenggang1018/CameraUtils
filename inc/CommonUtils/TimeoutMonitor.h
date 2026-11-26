/*
 * @Description: TimeoutMonitor Impl
 */

#ifndef INC_TIMEOUTMONITOR_H
#define INC_TIMEOUTMONITOR_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <thread>
#include <type_traits>

#include "ScopedUtils.h"

template <class Fn, class TimeoutCb, class... Args>
decltype(auto) TimeoutImpl(const std::string& fName, int32_t timeMs, Fn&& fn, TimeoutCb&& timeoutCb, Args&&... args)
{
    using R = std::invoke_result_t<Fn, Args...>;
    // Capture fn and args by value (moved) into a packaged_task that lives on the heap via
    // std::thread, so the worker thread owns its own copies and can safely outlive this call
    // after detach. This also avoids std::async's future-destructor blocking semantics: the
    // future returned by packaged_task has no such blocking destructor, so on timeout the caller
    // returns promptly instead of blocking until the monitored function finishes.
    std::packaged_task<R()> task([fn = std::forward<Fn>(fn), ... args = std::forward<Args>(args)]() mutable -> R {
        return std::invoke(fn, std::forward<Args>(args)...);
    });
    std::future<R> fu = task.get_future();
    std::thread worker(std::move(task));

    std::future_status status = fu.wait_for(std::chrono::milliseconds(timeMs));

    switch (status) {
    case std::future_status::ready:
        worker.join();
        return fu.get(); // get the result of the function in time
    case std::future_status::timeout:
        PV(fName, "timeout: ", timeMs);
        timeoutCb();     // call timeout callback
        worker.detach(); // background task keeps running on its own copies; this call returns now
        break;
    default:
        PV(fName, "unknown status"); // should never happen
        worker.detach();
        break;
    }

    // when timeout happens, we return a default value. which requires the return type to be default
    // constructible.
    if constexpr (std::is_same_v<R, void>) {
        return;
    } else if constexpr (std::is_integral_v<R>) {
        return static_cast<R>(-1);
    } else {
        static_assert(std::is_default_constructible_v<R>, "Type R must be default constructible");
        return R{};
    }
}

// Timeout listening is performed on the input function.

// fName: function name
// timeMs: maximum waiting time(ms)
// fn: function to be monitored;
// timeoutCb: timeout callback function when timeout happens;
// args: parameter of the function fn.
//
// Argument semantics: args are captured by value (lvalues are copied, rvalues are
// moved) so the detached worker thread owns its own copies and never references the
// caller's stack after TimeoutImpl returns. To keep reference semantics (have fn
// mutate a caller-owned object), wrap the argument in std::ref, e.g.
//   TimeoutMonitor("Foo", 3000, Foo, cb, std::ref(g_i));
// std::invoke unwraps the reference_wrapper back to a reference. The caller MUST
// guarantee the referenced object outlives the background thread (a global/static or
// heap object), otherwise the detached task dereferences freed memory.
template <class Fn, class TimeoutCb, class... Args>
decltype(auto) TimeoutMonitor(const std::string& fName, int32_t timeMs, Fn&& fn, TimeoutCb&& timeoutCb, Args&&... args)
{
    static_assert(std::is_invocable_v<Fn, Args...>);
    static_assert(std::is_invocable_v<TimeoutCb>);
    SCOPED_TIMER();
    return TimeoutImpl(fName, timeMs, std::forward<Fn>(fn), std::forward<TimeoutCb>(timeoutCb),
                       std::forward<Args>(args)...);
}

// 这里偏特化一个常用的场景，超时后直接调用linux的abort()终止进程
// （一般用于HIDL接口的IPC调用服务端接口的方法，超时须abort()防止IPC超时的场景）
template <class Fn, class... Args>
decltype(auto) TimeoutAbortMonitor(const std::string& fName, int32_t timeout, Fn&& fn, Args&&... args)
{
    // abort(), Actively kill process
    return TimeoutMonitor(fName, timeout, std::forward<Fn>(fn), abort, std::forward<Args>(args)...);
}

#endif // INC_TIMEOUTMONITOR_H