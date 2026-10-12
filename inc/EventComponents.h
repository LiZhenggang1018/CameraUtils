// @Description: Event componets including: sender receiver

#ifndef EVENTCOMPONENTS_H
#define EVENTCOMPONENTS_H

#include <memory>
#include <string>

#include "EventDispatcher.h"

class EventSender : public IEventSender, public Counter<EventSender>
{
public:
    explicit EventSender(const std::string& name) : name_(name)
    {
    }
    ~EventSender() override = default;
    DISALLOW_COPY_AND_MOVE(EventSender);

    std::string GetName() const override
    {
        return name_;
    }

    void SendEvent(const EventVariant& event) override
    {
        EventDispatcher::GetInstance().DispatchEvent(event);
    }

private:
    std::string name_{};
};

// EventReceiver Instance's life circle must be managed by std::shared_ptr or std::weak_ptr
class EventReceiver : public IEventReceiver,
                      public std::enable_shared_from_this<EventReceiver>,
                      public Counter<EventReceiver>
{
public:
    explicit EventReceiver(const std::string& name) : name_(name)
    {
    }
    ~EventReceiver() override = default;
    DISALLOW_COPY_AND_MOVE(EventReceiver);

    std::string GetName() const override
    {
        return name_;
    }

    // 注册特定事件类型的监听器
    template <typename EventType>
    void RegEvent(const ReceiverCb& cb)
    {
        EventDispatcher::GetInstance().AddListener<EventType>(
            shared_from_this()); // 这里拷贝了shared_ptr<EventReceiver>，会保活。
        cb_ = cb;
    }

    template <typename EventType>
    void UnregEvent()
    {
        EventDispatcher::GetInstance().RemoveListener<EventType>(shared_from_this());
    }

    void OnEvent(const EventVariant& event) override
    {
        cb_(event);
    }

private:
    std::string name_{};
    ReceiverCb cb_{};
};

#endif // EVENTCOMPONENTS_H