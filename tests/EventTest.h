#ifndef EVENTTEST_H
#define EVENTTEST_H

#include <cassert>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "EventComponents.h"
#include "EventDispatcher.h"

// example 1: shows sender receiver in same function
inline void TestBasicEventDelivery()
{
    auto sender = std::make_shared<EventSender>("S");
    auto receiver = std::make_shared<EventReceiver>("R");

    // 使用variant后，回调中可通过std::visit或std::get<>来处理具体事件，此处示例使用std::get<>
    auto cb = [](const EventVariant& event)
    {
        if (auto messageEvent = GetEvent<MessageEvent>(event))
        {
            PV("Rcv:", messageEvent->s_);
        }
    };

    receiver->RegEvent<MessageEvent>(cb);

    // 创建事件
    auto msg = MakeEvent<MessageEvent>("Hello World!");
    PV("send 1st");
    sender->SendEvent(msg); // receiver在cb()中接受到该事件

    PV("send 2nd");
    sender->SendEvent(msg); // receiver在cb()中接受到该事件

    receiver->UnregEvent<MessageEvent>();
}

// example 2: shows sender receiver in different module/class
struct S : public Counter<S>
{
public:
    S()
    {
        m_s = std::make_shared<EventSender>("S");
    };

    void Send()
    {
        auto msg = MakeEvent<MessageEvent>("Hello World!");
        PV("send 1st");
        m_s->SendEvent(msg);
        PV("send 2nd");
        m_s->SendEvent(msg);
    }

    ~S() {};

private:
    std::shared_ptr<EventSender> m_s{};
};

struct R : public Counter<R>
{
public:
    R()
    {
        m_r = std::make_shared<EventReceiver>("R");
        Reg(); // Receiver can RegEvent at ctor and UnregEvent at dtor;
    };

    void Cb(const EventVariant& event)
    {
        if (auto messageEvent = GetEvent<MessageEvent>(event))
        {
            PV("Rcv:", messageEvent->s_);
        }
    }

    void Reg()
    {
        m_r->RegEvent<MessageEvent>([this](const EventVariant& event) { Cb(event); });
    }

    ~R()
    {
        m_r->UnregEvent<MessageEvent>();
    };

private:
    std::shared_ptr<EventReceiver> m_r{};
};

inline void TestEventInTwoModule()
{
    auto s = std::make_shared<S>();
    auto r = std::make_shared<R>();
    // r->Reg();  // receiver reg event first, r has already registered event at it's ctor;
    s->Send(); // sender send event later
}

inline void TestEvent()
{
    TestBasicEventDelivery();
    TestEventInTwoModule();
}

#endif // !EVENTTEST_H
