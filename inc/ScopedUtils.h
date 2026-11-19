/*
 * @Description: Scope Utils Header
 */

#ifndef INC_SCOPEDUTILS_H
#define INC_SCOPEDUTILS_H

#include <chrono>
#include <deque>
#include <string>

#include "CustomLog.h"

#ifndef __FILE_NAME__
#define __FILE_NAME__ (__FILE__)
#endif

// used to generate a unique variable name with a line number
#define SCOPEUTILS_LINENAME_CAT(name, line) name##line
#define SCOPEUTILS_LINENAME(name, line) SCOPEUTILS_LINENAME_CAT(name, line)

#ifndef SCOPED_LOGGER
#define SCOPED_LOGGER()                                      \
    ScopedLogger SCOPEUTILS_LINENAME(ScopedLogger, __LINE__) \
    {                                                        \
        __FILE_NAME__, __LINE__, __FUNCTION__                \
    }
#endif
#ifndef SCOPED_TIMER
#define SCOPED_TIMER()                                     \
    ScopedTimer SCOPEUTILS_LINENAME(ScopedTimer, __LINE__) \
    {                                                      \
        __FILE_NAME__, __LINE__, __FUNCTION__              \
    }
#endif
/*
ScopedTimer: automatically log enter and exit and time duration of a scope;
usage:
{
    ScopedTimer st{"FuncName"}; // don't write like this ScopedTimer {"FuncName"}; nameless object,
print time of 0 this_thread::sleep_for(seconds(2));	//some time-consuming action which need to
calcute its runing time;
}
print FuncName : 2000000 us
*/
class ScopedTimer {
public:
    using ClockType = std::chrono::steady_clock;

    explicit ScopedTimer(const char* file, int line, const char* func)
        : file_(file),
          line_(line),
          func_(func),
          start_{ClockType::now()}
    {
        if (file_ != nullptr && func_ != nullptr) {
            PV(file_, ": L", line_, func_, " () begin");
            // LOGI("%s:%d %s() begin", file_, line_, func_);
        }
    };

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

    ~ScopedTimer()
    {
        if (file_ != nullptr && func_ != nullptr) {
            auto stop = ClockType::now();
            auto duration = stop - start_;
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
            PV(file_, ": L", line_, func_, " () end, cost time: ", us, " us");
            // LOGI("%s:%d %s() end, cost time: %lld us", file_, line_, func_, us);
        }
    }

private:
    const char* file_{};
    int line_{};
    const char* func_{};
    ClockType::time_point start_;
};

/*
 * ScopedLogger: automatically log enter and exit of a scope;
 * usage:
 * ScopedLogger sl{};   // log: enter default, exit default;
 * ScopedLogger sl{ "customFuncName" }; // log: enter customFuncName, exit customFuncName;
 */
class ScopedLogger {
public:
    explicit ScopedLogger(const char* file, int line, const char* func)
        : file_(file),
          line_(line),
          func_(func)
    {
        if (file_ != nullptr && func_ != nullptr) {
            PV(file_, ": L", line_, func_, " () enter");
            // LOGI("%s:%d %s() enter", file_, line_, func_);
        }
    };

    ScopedLogger(const ScopedLogger&) = delete;
    ScopedLogger(ScopedLogger&&) = delete;
    ScopedLogger& operator=(const ScopedLogger&) = delete;
    ScopedLogger& operator=(ScopedLogger&&) = delete;

    ~ScopedLogger()
    {
        if (file_ != nullptr && func_ != nullptr) {
            PV(file_, ": L", line_, func_, " () exit");
            // LOGI("%s:%d %s() exit", file_, line_, func_);
        }
    }

private:
    const char* file_{};
    int line_{};
    const char* func_{};
};

template <typename Func>
class ScopeGuard {
public:
    ScopeGuard(Func&& fn)
        : fn_(std::forward<Func>(fn)),
          active_(true)
    {}
    ~ScopeGuard()
    {
        if (active_) {
            fn_();
        }
    }

    void Dismiss() { active_ = false; }

private:
    Func fn_;
    bool active_{};
};

// tag dispatch
struct ScopeGuardOnExit {};
template <typename Func>
static inline ScopeGuard<Func> operator+(ScopeGuardOnExit /*tag*/, Func&& fn)
{
    return ScopeGuard<Func>(std::forward<Func>(fn));
}

/* 在函数退出时调用ON_SCOPE_EXIT块内容，可用于做一下收尾动作（释放资源），避免因为某些异常忘记收尾
* example:
*  void process() {
*      char* buf = (char*)malloc(512);
*      ON_SCOPE_EXIT { // 当函数退出时执行ON_SCOPE_EXIT块代码，资源释放可以统一放到这里
*          printf("function exit, release buf");
*          free (buf);
*          buf = nullptr;
*      };
*
*      if (! process1(buf)) {
*          // free(buf);
*          // buf = nullptr;
*          return; // return自动调用ON_SCOPE_EXIT块内容，避免忘记释放资源free(buf)
*      }

*      if (! process2(buf)) {
*          // free(buf);
*          // buf = nullptr;
*          return;
*      }

*      if (! process3(buf)) {
*          // free(buf);
*          // buf = nullptr;
*          return;
*      }
*      // ...
*      // free(buf);
*      // buf = nullptr;
*      return;
*  }
*/

#define ON_SCOPE_EXIT auto __onGuardExit__ = ScopeGuardOnExit{} + [&]

#define CANCEL_SCOPE_EXIT __onGuardExit__.Dismiss()

#endif