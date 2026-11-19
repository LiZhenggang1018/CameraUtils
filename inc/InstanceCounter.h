/*
 * @Description: CRTP pattern Counter impl which gets Instance count
 */

#ifndef INC_INSTANCECOUNTER_H
#define INC_INSTANCECOUNTER_H

#include <atomic>
#include <iostream>

#include "CustomLog.h"

// 实例计数器模板类
template <typename Derived>
class Counter {
private:
    inline static std::atomic<int> cnt_{};

public:
    Counter()
    {
        ++cnt_;
        // static_cast<Derived*>(this) cause CFI check fail,
        // 构造函数调用顺序是先父类，后成员，最后自己/子类。所以此时Derived部分还未构造完成。
        // this转成 Derived* 会触发CFI检查失败。
        // PV(typeid(Derived).name(), "ctor at D", static_cast<Derived*>(this),",current count =",
        // cnt);
        // static_cast<Derived*>(this) 跟 this
        // 的地址打印不一样，差8个字节，可能是因为虚表指针的原因。
        PV(typeid(Derived).name(), "ctor at", this, ",current count =", cnt_);
    }

    // 拷贝构造函数
    Counter(const Counter& /*other*/)
    {
        ++cnt_;
        // PV(typeid(Derived).name(), "copy ctor at", static_cast<Derived*>(this), ",current count
        // =", cnt);
        PV(typeid(Derived).name(), "copy ctor at", this, ",current count =", cnt_);
    }

    // 移动构造函数
    Counter(Counter&& /*other*/) noexcept
    {
        ++cnt_;
        // PV(typeid(Derived).name(), "move ctor at", static_cast<Derived*>(this), ",current count
        // =", cnt);
        PV(typeid(Derived).name(), "move ctor at", this, ",current count =", cnt_);
    }

    virtual ~Counter()
    {
        --cnt_;
        // PV(typeid(Derived).name(), "~ctor at", static_cast<Derived*>(this), ",current count =",
        // cnt);
        PV(typeid(Derived).name(), "~ctor at", this, ",current count =", cnt_);
    }

    // 获取当前实例数量
    static int GetInstanceCount() { return cnt_.load(); }

    // 禁止赋值操作符(拷贝赋值，移动赋值)，避免计数错误
    Counter& operator=(const Counter&) = delete;
    Counter& operator=(Counter&&) = delete;
};

#endif // INC_INSTANCECOUNTER_H
