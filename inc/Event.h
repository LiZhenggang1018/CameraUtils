// @Description: event mechanism that allows two independent C++ modules to communicate through
// events. sender sends events, and receiver registers to listen for and receive events.

#ifndef EVENT_H
#define EVENT_H

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include "CheckUtils.h"
#include "CustomLog.h"
#include "InstanceCounter.h"
#include "NoCopyableMacro.h"
#include "Singleton.h"

// 前向声明具体事件类型
struct MessageEvent;

// 使用variant定义所有事件类型
using EventVariant = std::variant<std::monostate, // 占位，表示空事件
                                  MessageEvent
                                  // 可以继续添加其他事件类型...
                                  >;

// 事件发送者接口
struct IEventSender
{
public:
    virtual ~IEventSender() = default;
    virtual void SendEvent(const EventVariant& event) = 0;
    virtual std::string GetName() const = 0;
};

// 事件监听器接口
struct IEventReceiver
{
    virtual ~IEventReceiver() = default;
    virtual void OnEvent(const EventVariant& event) = 0;
    virtual std::string GetName() const = 0;
};

using ReceiverCb = std::function<void(const EventVariant& event)>;

// 自定义事件类型定义
struct MessageEvent
{
    std::string s_{};
};

// 辅助函数来检查事件类型
template <typename T>
bool IsEventType(const EventVariant& event)
{
    return std::holds_alternative<T>(event);
}

// 安全获取事件内容
template <typename T>
const T* GetEvent(const EventVariant& event)
{
    return std::get_if<T>(&event);
}

// 创建事件的辅助函数
template <typename T, typename... Args>
EventVariant MakeEvent(Args&&... args)
{
    return T{std::forward<Args>(args)...};
}

// 使用函数模板实例的地址作为类型标识符
template <typename T>
void EventTypeTag()
{
}

template <typename T>
size_t GetEventTypeId()
{
    static_assert(!std::is_same_v<T, std::monostate>, "Cannot use monostate as event type");

    // 每个函数模板实例都有唯一的地址
    static const auto tag_ptr = &EventTypeTag<T>;
    return reinterpret_cast<size_t>(tag_ptr);
}

//// C++17环境编译不过，C++20编译OK
//// 编译时类型标识符 - 基于variant索引,接受一个类型T，返回这个类型在EventVariant中的index();从0开始
// template<typename T>
// struct EventTypeId {
//     static constexpr size_t value = []() constexpr {
//         // 创建一个临时的variant来获取类型T的索引
//         EventVariant v = T{};
//         return v.index(); // index()在C++20 是constexpr，故编译OK；在C++17不是constexpr，编译不过
//         }();
// };
//
//// 获取事件类型的唯一标识符
////
///利用variant的索引index()。因为EventVariant包含了所有的事件类型，所以每个类型在EventVariant中的索引是固定的，并且是编译时可知的。
////
///标识是一个从0开始的小整数，易于阅读，且编译时确定。如果EventVariant改变，则所有类型的标识都会改变。
// template<typename EventType>
// constexpr size_t GetEventTypeId() {
//     static_assert(!std::is_same_v<EventType, std::monostate>, "Cannot use monostate as event
//     type"); return EventTypeId<EventType>::value;
// }

#endif // !EVENT_H