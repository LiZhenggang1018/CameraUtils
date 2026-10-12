#ifndef SYCNAGENCYTEST_H
#define SYCNAGENCYTEST_H

#include "AllUsedHeadFiles.h"
#include "CustomLog.h"
#include "SyncAgency.h"

static constexpr auto SYNC_SLEEP_TIME = std::chrono::milliseconds(1000);

// 如果是多个不同的so之间要互相sync机制，应该将SyncAgency单独编成一个单例的so，
// 其余so都去通过dlopen的方式获取此so实例，并实现sync通信。

struct Sender
{
public:
    Sender()
    { // 构造函数中注册 SyncId::CUSTOM_SIGNAL，并设置状态为 RUNNABLE
        m_syncNotifier = SyncAgency::GetInstance().Register(SyncId::CUSTOM_SIGNAL);
        m_syncNotifier->SetState(SyncState::RUNNABLE);
    };
    ~Sender()
    { // 析构函数中取消注册 SyncId::CUSTOM_SIGNAL
        if (m_syncNotifier)
        {
            SyncAgency::GetInstance().UnRegister(SyncId::CUSTOM_SIGNAL, m_syncNotifier);
        }
    };
    void Foo()
    { // Foo() shall trigger SyncId::CUSTOM_SIGNAL by m_syncNotifier->SetState(SyncState::DONE);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(1500)); // 这里设置不同时间如1500/15000 ms来 测试case1&2
        m_syncNotifier->SetState(
            SyncState::DONE); // 如下触发信号SyncId::CUSTOM_SIGNAL，通知接收方结束等待。
        // std::this_thread::sleep_for(SYNC_SLEEP_TIME);
        // //即使SetState(SyncState::DONE)后立刻UnRegister，WaitSync依然能正常拿到这个信息并结束
    };

private:
    std::shared_ptr<SyncNotifier> m_syncNotifier{};
};

struct Receiver
{
public:
    // Receiver::Bar() shall wait Sender::Foo() to trigger SyncId::CUSTOM_SIGNAL by
    // m_syncNotifier->SetState(SyncState::DONE);
    void Bar()
    {
        std::this_thread::sleep_for(SYNC_SLEEP_TIME);
        // 等待信号量SyncId::CUSTOM_SIGNAL，最大等待时间TIMEOUT_5S
        SyncAgency::GetInstance().WaitSync(SyncId::CUSTOM_SIGNAL, TIMEOUT_5S);
        // 如果此时 外面还没有Register(CUSTOM_SIGNAL);
        // 则WaitSync()会立马运行完成，打印notifiers.empty() 如果此时
        // 外面已经Register(CUSTOM_SIGNAL);但是Sender::Foo()没有触发SyncId::CUSTOM_SIGNAL，则会一直等待，直到超时。
        PV("End Bar");
    };
};

inline void TestSyncAgency()
{
    auto s = std::make_shared<Sender>();
    auto r = std::make_shared<Receiver>();
    std::thread t1{
        [&]()
        {
            r->Bar();
        }}; // Receiver::Bar() shall wait until Sender::Foo() to trigger SyncId::CUSTOM_SIGNAL
    std::thread t2{[&]() { s->Foo(); }};
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    t1.join();
    t2.join();
}

#endif
