/*
 * @Description: TimeoutMonitor Impl
 */

#ifndef TIMEOUTMONITOR_H
#define TIMEOUTMONITOR_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>

#include "ScopedUtils.h"

template <class Fn, class TimeoutCb, class... Args>
decltype(auto)
TimeoutImpl(std::string fName, int32_t timeMs, Fn&& fn, TimeoutCb&& timeoutCb, Args&&... args)
{
    using R = std::invoke_result_t<Fn, Args...>;
    // always capture by reference [&] and forward arguments to avoid template args' cvr(const
    // reference volitile) attribute is decayed
    std::future<R> fu = std::async(
        std::launch::async, [&] { return std::forward<Fn>(fn)(std::forward<Args>(args)...); });
    std::future_status status = fu.wait_for(std::chrono::milliseconds(timeMs));

    switch (status)
    {
    case std::future_status::ready:
        return fu.get(); // get the result of the function in time
    case std::future_status::timeout:
        PV(fName, "timeout: ", timeMs);
        timeoutCb(); // call timeout callback
        break;
    default:
        PV(fName, "unknown status"); // should never happen
        break;
    }

    // when timeout happens, we return a default value. which requires the return type to be default
    // constructible.
    if constexpr (std::is_same_v<R, void>)
    {
        return;
    }
    else if constexpr (std::is_integral_v<R>)
    {
        return static_cast<R>(-1);
    }
    else
    {
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
template <class Fn, class TimeoutCb, class... Args>
decltype(auto)
TimeoutMonitor(std::string fName, int32_t timeMs, Fn&& fn, TimeoutCb&& timeoutCb, Args&&... args)
{
    static_assert(std::is_invocable_v<Fn, Args...>);
    static_assert(std::is_invocable_v<TimeoutCb>);
    SCOPED_TIMER();
    return TimeoutImpl(fName,
                       timeMs,
                       std::forward<Fn>(fn),
                       std::forward<TimeoutCb>(timeoutCb),
                       std::forward<Args>(args)...);
}

// 这里偏特化一个常用的场景，超时后直接调用linux的abort()终止进程
// （一般用于HIDL接口的IPC调用服务端接口的方法，超时须abort()防止IPC超时的场景）
template <class Fn, class... Args>
decltype(auto) TimeoutAbortMonitor(std::string fName, int32_t timeout, Fn&& fn, Args&&... args)
{
    // abort(), Actively kill process
    return TimeoutMonitor(fName, timeout, std::forward<Fn>(fn), abort, std::forward<Args>(args)...);
}

#endif // TIMEOUTMONITOR_H