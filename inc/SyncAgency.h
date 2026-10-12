/*
 * @Description: SyncAgency mechanism allows independent C++ module to have happens-before/busy wait
 * time dependence
 */

#ifndef SYNCAGENCY_H
#define SYNCAGENCY_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "NoCopyableMacro.h"

namespace
{
constexpr int64_t TIMEOUT_5S = 5000;
}

enum class SyncId : int32_t
{
    CUSTOM_SIGNAL = 1,
    // add custom signal below
};

enum class SyncState : uint8_t
{
    RUNNABLE,
    DONE,
};

namespace SyncUtils
{
// dump func to print SyncId as string
static const char* ToString(SyncId id)
{
    static const std::unordered_map<SyncId, std::string> syncIdToString = {
        {SyncId::CUSTOM_SIGNAL, "CUSTOM_SIGNAL"},
        // Add more mappings here
    };
    auto it = syncIdToString.find(id);
    return it != syncIdToString.end() ? it->second.c_str() : "UNKNOWN_SYNC_ID";
}
}; // namespace SyncUtils

class SyncNotifier final
{
public:
    explicit SyncNotifier(SyncId syncId) : m_syncId(syncId) {};
    ~SyncNotifier() = default;
    DISALLOW_COPY_AND_MOVE(SyncNotifier);

    void SetState(SyncState state)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (state == SyncState::DONE)
        {
            PV(SyncUtils::ToString(m_syncId), "Done");
            m_isReady = true;
            m_cv.notify_all();
        }
        else
        {
            m_isReady = false;
        }
    }

    void Wait(int64_t timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        PV(SyncUtils::ToString(m_syncId), "m_isReady", m_isReady);
        m_cv.wait_for(lock, std::chrono::milliseconds(timeout), [this]() { return m_isReady; });
    }

private:
    SyncId m_syncId{};
    mutable std::mutex m_mutex{};
    bool m_isReady{true};
    std::condition_variable m_cv{};
};

class SyncAgency final : public Singleton<SyncAgency>
{
public:
    std::shared_ptr<SyncNotifier> Register(SyncId syncId)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto notifier = std::make_shared<SyncNotifier>(syncId);
        m_notifiers.insert({syncId, notifier});
        PrintNotifiers();
        return notifier;
    }

    void UnRegister(SyncId syncId, std::shared_ptr<SyncNotifier> notifier)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        CAR(!notifier);
        // UnRegister中会 先强制设置SyncState::DONE
        notifier->SetState(SyncState::DONE); // unexpected stop, we should notify done
        for (auto iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
        {
            if (iter->first == syncId && iter->second == notifier)
            {
                m_notifiers.erase(iter);
                break;
            }
        }
        notifier.reset();
    }

    void WaitSync(SyncId syncId, int64_t timeout = 0)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        CAR(m_notifiers.count(syncId) == 0);
        auto timeLeft = timeout;
        auto range = m_notifiers.equal_range(syncId);
        lock.unlock();

        // 遍历multimap中所有key为syncId的value/syncNotifier项,依次调用每个syncNotifier的Wait()接口
        for (auto&& i = range.first; i != range.second; ++i)
        {
            if (timeLeft <= 0)
            {
                PV("timeLeft <= 0");
                continue;
            }
            auto&& syncNotifier = i->second;
            if (!syncNotifier)
            {
                PV("!syncNotifier");
                continue;
            }
            auto begin = std::chrono::steady_clock::now();
            syncNotifier->Wait(timeLeft);

            auto elapsedOnce = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin);
            timeLeft -= elapsedOnce.count(); // 下一次次loop的超时阈值，减去本次Wait消耗的时间
        }
    }

    void PrintNotifiers() const
    {
        if (m_notifiers.empty())
        {
            PV("notifiers.empty()");
        }
        for (auto&& [id, sp] : m_notifiers)
        {
            PV("syncId:", static_cast<int>(id), "sync Notifier addr:", sp.get());
        }
    }

private:
    // 同一个SyncId可以对应多个SyncNotifier实例，即有多个Receiver在等待同一个SyncId
    std::multimap<SyncId, std::shared_ptr<SyncNotifier>> m_notifiers{};
    mutable std::mutex m_mutex{};
};

#endif