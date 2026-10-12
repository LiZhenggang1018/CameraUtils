//@Description: Event Dispatcher Singleton used to dispatch events
// EventDispatcher Sinlgeton impl should ensure only one instance(same addr) in different so files

#ifndef EVENTDISPATCHER_H
#define EVENTDISPATCHER_H

#include <atomic>
#include <iostream>
#include <memory>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Event.h"
#include "ThreadPool.h"

// 事件分发器接口
class IEventDispatcher
{
public:
    virtual ~IEventDispatcher() = default;
    virtual void DispatchEvent(const EventVariant& event) = 0;
    virtual void Clear() = 0;
    virtual size_t GetTotalListenerCount() const = 0;
};

// 线程安全的事件分发器实现（单例模式）
class EventDispatcher : public IEventDispatcher, public Singleton<EventDispatcher>
{
private:
    // 监听器条目
    struct ListenerEntry
    {
        std::weak_ptr<IEventReceiver> listener{};
        std::string name{};

        explicit ListenerEntry(const std::shared_ptr<IEventReceiver>& l)
            : listener(l), name(l->GetName())
        {
        }
        bool Expired() const
        {
            return listener.expired();
        }
        std::shared_ptr<IEventReceiver> Lock() const
        {
            return listener.lock();
        }
    };

public:
    // 添加特定事件类型的监听器
    template <typename EventType>
    bool AddListener(const std::shared_ptr<IEventReceiver>& listener)
    {
        CARR(!listener, false);

        std::unique_lock lock(listenersMtx_);
        const size_t eventTypeId = GetEventTypeId<EventType>();
        auto& entries = listeners_[eventTypeId];

        // 检查是否已存在
        for (const auto& entry : entries)
        {
            if (auto e = entry.Lock())
            {
                CARR(e == listener, false);
            }
        }

        entries.emplace_back(listener);
        PV("Listener", listener->GetName(), "registered for event type:", eventTypeId);
        return true;
    }

    // 移除特定事件类型的监听器
    template <typename EventType>
    bool RemoveListener(const std::shared_ptr<IEventReceiver>& listener)
    {
        CARR(!listener, false);

        std::unique_lock lock(listenersMtx_);
        const size_t eventTypeId = GetEventTypeId<EventType>();
        auto it = listeners_.find(eventTypeId);
        CARR(it == listeners_.end(), false);

        auto& entries = it->second;
        entries.erase(std::remove_if(entries.begin(),
                                     entries.end(),
                                     [&](const ListenerEntry& entry)
                                     {
                                         if (auto l = entry.Lock())
                                         {
                                             return l == listener;
                                         }
                                         return false;
                                     }),
                      entries.end());

        if (entries.empty())
        {
            PV("entries empty, remove all");
            listeners_.erase(it);
        }
        return true;
    }

    void DispatchEvent(const EventVariant& event) override
    {
        // 使用visit来分发事件
        std::visit(
            [this](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (!std::is_same_v<T, std::monostate>)
                {
                    this->DispatchToListeners<T>(arg);
                }
            },
            event);
    }

    // 打印当前状态（用于调试）
    void PrintStatus() const
    {
        std::shared_lock lock(listenersMtx_);
        PV("Total event types : ", listeners_.size(), "Total listeners: ", GetTotalListenerCount());
        for (const auto& [typeId, entries] : listeners_)
        {
            PV("Event type ID: ", typeId, " - ", entries.size(), " listeners");
        }
    }

    void Clear() override
    {
        std::unique_lock lock(listenersMtx_);
        listeners_.clear();
        std::cout << "All listeners cleared from EventDispatcher" << std::endl;
    }

    size_t GetTotalListenerCount() const override
    {
        std::shared_lock lock(listenersMtx_);
        size_t count = 0;
        for (const auto& [typeId, entries] : listeners_)
        {
            count += entries.size();
        }
        return count;
    }

private:
    mutable std::shared_mutex listenersMtx_{};
    std::unordered_map<size_t, std::vector<ListenerEntry>> listeners_{};
    std::atomic<bool> isDispatching_{};
    inline static constexpr int numThreads_{8};
    ThreadPool pool_{numThreads_};

    // 分发事件到特定类型的监听器
    template <typename EventType>
    void DispatchToListeners(const EventType& event)
    {
        const size_t eventTypeId = GetEventTypeId<EventType>();

        std::shared_lock lock(listenersMtx_);
        isDispatching_ = true;

        auto it = listeners_.find(eventTypeId);
        if (it != listeners_.end())
        {
            // 复制监听器列表以避免在回调中修改导致的迭代器失效
            auto entries = it->second;
            lock.unlock(); // 释放锁，允许其他线程注册/移除监听器

            for (const auto& entry : entries)
            {
                if (auto listener = entry.Lock())
                {
                    // 同步调用Cb
                    // listener->OnEvent(EventVariant{ event });

                    // 异步调用Cb，改成在线程池中执行回调,以防止某个监听器阻塞整个分发过程
                    // 异步会拷贝EventType，要求可拷贝
                    static_assert(std::is_copy_constructible_v<EventType>,
                                  "EventType must be copy constructible");

                    pool_.EnqueMsg(
                        [listener, event]()
                        {
                            listener->OnEvent(
                                EventVariant{event}); // 将具体事件包装成variant传递给监听器
                        });
                }
            }
        }

        isDispatching_ = false;
    }

    void CleanupExpiredListeners(std::vector<ListenerEntry>& entries)
    {
        entries.erase(std::remove_if(entries.begin(),
                                     entries.end(),
                                     [](const ListenerEntry& entry) { return entry.Expired(); }),
                      entries.end());
    }
};

#endif // EVENTDISPATCHER_H