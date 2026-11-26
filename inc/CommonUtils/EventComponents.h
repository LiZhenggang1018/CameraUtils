// @Description: Event componets including: sender receiver

#ifndef INC_EVENTCOMPONENTS_H
#define INC_EVENTCOMPONENTS_H

#include <memory>
#include <mutex>
#include <string>

#include "EventDispatcher.h"

class EventSender : public IEventSender, public Counter<EventSender> {
public:
    explicit EventSender(const std::string& name)
        : name_(name)
    {}
    ~EventSender() override = default;
    DISALLOW_COPY_AND_MOVE(EventSender);

    [[nodiscard]] std::string GetName() const override { return name_; }

    void SendEvent(const EventVariant& event) override { EventDispatcher::GetInstance().DispatchEvent(event); }

private:
    std::string name_;
};

// EventReceiver Instance's life circle must be managed by std::shared_ptr or std::weak_ptr
class EventReceiver : public IEventReceiver,
                      public std::enable_shared_from_this<EventReceiver>,
                      public Counter<EventReceiver> {
public:
    explicit EventReceiver(const std::string& name)
        : name_(name)
    {}
    ~EventReceiver() override = default;
    DISALLOW_COPY_AND_MOVE(EventReceiver);

    [[nodiscard]] std::string GetName() const override { return name_; }

    // 注册特定事件类型的监听器
    template <typename EventType>
    void RegEvent(const ReceiverCb& cb)
    {
        // 先持锁写入 cb_，再注册监听器，避免「监听器已生效但 cb_ 尚未赋值」的
        // 窗口导致 OnEvent 调用到空 std::function。
        {
            std::unique_lock lock(cbMutex_);
            cb_ = cb;
        }
        EventDispatcher::GetInstance().AddListener<EventType>(
            shared_from_this()); // 这里拷贝了shared_ptr<EventReceiver>，会保活。
    }

    template <typename EventType>
    void UnregEvent()
    {
        EventDispatcher::GetInstance().RemoveListener<EventType>(shared_from_this());
    }

    void OnEvent(const EventVariant& event) override
    {
        // 持锁拷贝 cb_ 后再调用，消除与 RegEvent 写 cb_ 的数据竞争；判空兜底，
        // 避免极端时序下空 std::function 调用抛出 bad_function_call。
        ReceiverCb cb;
        {
            std::unique_lock lock(cbMutex_);
            cb = cb_;
        }
        if (cb) {
            cb(event);
        }
    }

private:
    std::string name_;
    ReceiverCb cb_;
    mutable std::mutex cbMutex_;
};

#endif // INC_EVENTCOMPONENTS_H