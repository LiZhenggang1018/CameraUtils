/*
 * @Description: BlockingDeque and NonBlockingDeque
 */

#ifndef INC_DOUBLEENDEDBLOCKINGQUEUE_H
#define INC_DOUBLEENDEDBLOCKINGQUEUE_H

#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

#include "NoCopyableMacro.h"

// 线程安全的 支持阻塞/非阻塞式的生产者/消费者 双端队列
template <typename T, bool IS_BLOCKING>
class OptionalBlockingDeque {
public:
    explicit OptionalBlockingDeque(int maxSize, int timeoutMs = DEFAULT_TIMEOUT_MS)
        : maxSize_{maxSize},
          timeoutMs_{std::chrono::milliseconds{timeoutMs}}
    {}

    DISALLOW_COPY_AND_MOVE(OptionalBlockingDeque);

    OptionalBlockingDeque() = delete;
    // 默认析构
    ~OptionalBlockingDeque() = default;

    // 开始队列操作
    void Start()
    {
        std::lock_guard lock(mutex_);
        isRunning_ = true;
        cond_.notify_all(); // 通知所有等待的线程
    }

    // 停止队列操作
    void Stop()
    {
        std::lock_guard lock(mutex_);
        isRunning_ = false;
        cond_.notify_all(); // 通知所有等待的线程
    }

    // 清空队列
    void Clear()
    {
        std::lock_guard lock(mutex_);
        deque_.clear();
    }

    // 检查队列是否为空
    bool IsEmpty() const
    {
        std::lock_guard lock(mutex_);
        return deque_.empty();
    }

    // 获取队列的大小
    size_t Size() const
    {
        std::lock_guard lock(mutex_);
        return deque_.size();
    }

    // 入队操作（从队尾入队）
    bool PushBack(const T& t) { return Enqueue(t, true); }

    // 入队操作（从队首入队）
    bool PushFront(const T& t) { return Enqueue(t, false); }

    // 出队操作（从队尾出队）
    bool PopBack(T& t) { return Dequeue(t, true); }

    // 出队操作（从队首出队）
    bool PopFront(T& t) { return Dequeue(t, false); }

private:
    std::deque<T> deque_{};
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    int maxSize_{}; // deque最大size
    bool isRunning_{};
    std::chrono::milliseconds timeoutMs_{}; // 阻塞调用时最大超时时间 timeoutMs
    static constexpr int DEFAULT_TIMEOUT_MS{1000};

    // 通用入队操作
    bool Enqueue(const T& value, bool back)
    {
        if constexpr (IS_BLOCKING) {
            std::unique_lock lock(mutex_);
            if (!cond_.wait_for(lock, timeoutMs_, [this] { return isRunning_ && deque_.size() < maxSize_; })) {
                return false; // 超时，返回false
            }
            if (back) {
                deque_.push_back(value);
            } else {
                deque_.push_front(value);
            }
            cond_.notify_all();
            return true;
        } else {
            std::lock_guard lock(mutex_);
            if (deque_.size() >= maxSize_) {
                return false; // 队列已满，返回false
            }
            if (back) {
                deque_.push_back(value);
            } else {
                deque_.push_front(value);
            }
            cond_.notify_all();
            return true;
        }
    }

    // 通用出队操作
    bool Dequeue(T& value, bool back)
    {
        if constexpr (IS_BLOCKING) {
            std::unique_lock lock(mutex_);
            if (!cond_.wait_for(lock, timeoutMs_, [this] { return !deque_.empty() || !isRunning_; })) {
                return false; // 超时，返回false
            }
            if (!deque_.empty()) {
                value = back ? std::move(deque_.back()) : std::move(deque_.front());
                if (back) {
                    deque_.pop_back();
                } else {
                    deque_.pop_front();
                }
                cond_.notify_all();
                return true;
            }
            return false;
        } else {
            std::lock_guard lock(mutex_);
            if (deque_.empty()) {
                return false; // 队列为空，返回false
            }
            value = back ? std::move(deque_.back()) : std::move(deque_.front());
            if (back) {
                deque_.pop_back();
            } else {
                deque_.pop_front();
            }
            cond_.notify_all();
            return true;
        }
    }
};

template <typename T>
using BlockingDeque = OptionalBlockingDeque<T, true>;

template <typename T>
using NonBlockingDeque = OptionalBlockingDeque<T, false>;

#endif // INC_DOUBLEENDEDBLOCKINGQUEUE_H
