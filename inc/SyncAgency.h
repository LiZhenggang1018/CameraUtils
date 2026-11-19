/*
 * @Description: SyncAgency mechanism allows independent C++ module to have happens-before/busy wait
 * time dependence
 */

#ifndef INC_SYNCAGENCY_H
#define INC_SYNCAGENCY_H

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

namespace {
constexpr int64_t TIMEOUT_5S = 5000;
}

enum class SyncId : int32_t {
    CUSTOM_SIGNAL = 1,
    // add custom signal below
};

enum class SyncState : uint8_t {
    RUNNABLE,
    DONE,
};

namespace SyncUtils {
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

class SyncNotifier final {
public:
    explicit SyncNotifier(SyncId syncId)
        : syncId_(syncId) {};
    ~SyncNotifier() = default;
    DISALLOW_COPY_AND_MOVE(SyncNotifier);

    void SetState(SyncState state)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state == SyncState::DONE) {
            PV(SyncUtils::ToString(syncId_), "Done");
            isReady_ = true;
            cv_.notify_all();
        } else {
            isReady_ = false;
        }
    }

    void Wait(int64_t timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        PV(SyncUtils::ToString(syncId_), "isReady_", isReady_);
        cv_.wait_for(lock, std::chrono::milliseconds(timeout), [this]() { return isReady_; });
    }

private:
    SyncId syncId_{};
    mutable std::mutex mutex_;
    bool isReady_{true};
    std::condition_variable cv_;
};

class SyncAgency final : public Singleton<SyncAgency> {
public:
    std::shared_ptr<SyncNotifier> Register(SyncId syncId)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto notifier = std::make_shared<SyncNotifier>(syncId);
        notifiers_.insert({syncId, notifier});
        PrintNotifiers();
        return notifier;
    }

    void UnRegister(SyncId syncId, std::shared_ptr<SyncNotifier> notifier)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        CAR(!notifier);
        // UnRegister中会 先强制设置SyncState::DONE
        notifier->SetState(SyncState::DONE); // unexpected stop, we should notify done
        for (auto iter = notifiers_.begin(); iter != notifiers_.end(); ++iter) {
            if (iter->first == syncId && iter->second == notifier) {
                notifiers_.erase(iter);
                break;
            }
        }
        notifier.reset();
    }

    void WaitSync(SyncId syncId, int64_t timeout = 0)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        CAR(notifiers_.count(syncId) == 0);
        auto timeLeft = timeout;
        auto range = notifiers_.equal_range(syncId);
        lock.unlock();

        // 遍历multimap中所有key为syncId的value/syncNotifier项,依次调用每个syncNotifier的Wait()接口
        for (auto&& i = range.first; i != range.second; ++i) {
            if (timeLeft <= 0) {
                PV("timeLeft <= 0");
                continue;
            }
            auto&& syncNotifier = i->second;
            if (!syncNotifier) {
                PV("!syncNotifier");
                continue;
            }
            auto begin = std::chrono::steady_clock::now();
            syncNotifier->Wait(timeLeft);

            auto elapsedOnce =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
            timeLeft -= elapsedOnce.count(); // 下一次次loop的超时阈值，减去本次Wait消耗的时间
        }
    }

    void PrintNotifiers() const
    {
        if (notifiers_.empty()) {
            PV("notifiers.empty()");
        }
        for (auto&& [id, sp] : notifiers_) {
            PV("syncId:", static_cast<int>(id), "sync Notifier addr:", sp.get());
        }
    }

private:
    // 同一个SyncId可以对应多个SyncNotifier实例，即有多个Receiver在等待同一个SyncId
    std::multimap<SyncId, std::shared_ptr<SyncNotifier>> notifiers_;
    mutable std::mutex mutex_;
};

#endif