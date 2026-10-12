/*
 * @Description: BlockingDeque and NonBlockingDeque
 */

#ifndef DOUBLE_ENDED_BLOCKING_QUEUE_H
#define DOUBLE_ENDED_BLOCKING_QUEUE_H

#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

#include "NoCopyableMacro.h"

// 线程安全的 支持阻塞/非阻塞式的生产者/消费者 双端队列
template <typename T, bool IS_BLOCKING>
class OptionalBlockingDeque
{
public:
    explicit OptionalBlockingDeque(int maxSize, int timeoutMs = DEFAULT_TIMEOUT_MS)
        : m_size{maxSize}, m_timeoutMs{std::chrono::milliseconds{timeoutMs}}
    {
    }

    DISALLOW_COPY_AND_MOVE(OptionalBlockingDeque);

    OptionalBlockingDeque() = delete;
    // 默认析构
    ~OptionalBlockingDeque() = default;

    // 开始队列操作
    void Start()
    {
        std::lock_guard lock(m_mtx);
        m_isRunning = true;
        m_cond.notify_all(); // 通知所有等待的线程
    }

    // 停止队列操作
    void Stop()
    {
        std::lock_guard lock(m_mtx);
        m_isRunning = false;
        m_cond.notify_all(); // 通知所有等待的线程
    }

    // 清空队列
    void Clear()
    {
        std::lock_guard lock(m_mtx);
        m_deque.clear();
    }

    // 检查队列是否为空
    bool IsEmpty() const
    {
        std::lock_guard lock(m_mtx);
        return m_deque.empty();
    }

    // 获取队列的大小
    size_t Size() const
    {
        std::lock_guard lock(m_mtx);
        return m_deque.size();
    }

    // 入队操作（从队尾入队）
    bool PushBack(const T& t)
    {
        return Enqueue(t, true);
    }

    // 入队操作（从队首入队）
    bool PushFront(const T& t)
    {
        return Enqueue(t, false);
    }

    // 出队操作（从队尾出队）
    bool PopBack(T& t)
    {
        return Dequeue(t, true);
    }

    // 出队操作（从队首出队）
    bool PopFront(T& t)
    {
        return Dequeue(t, false);
    }

private:
    std::deque<T> m_deque{};
    mutable std::mutex m_mtx{};
    std::condition_variable m_cond{};
    int m_size{}; // deque最大size
    bool m_isRunning{};
    std::chrono::milliseconds m_timeoutMs{}; // 阻塞调用时最大超时时间 timeoutMs
    static constexpr int DEFAULT_TIMEOUT_MS{1000};

    // 通用入队操作
    bool Enqueue(const T& value, bool back)
    {
        if constexpr (IS_BLOCKING)
        {
            std::unique_lock lock(m_mtx);
            if (!m_cond.wait_for(
                    lock, m_timeoutMs, [this] { return m_isRunning && m_deque.size() < m_size; }))
            {
                return false; // 超时，返回false
            }
            if (back)
            {
                m_deque.push_back(value);
            }
            else
            {
                m_deque.push_front(value);
            }
            m_cond.notify_all();
            return true;
        }
        else
        {
            std::lock_guard lock(m_mtx);
            if (m_deque.size() >= m_size)
            {
                return false; // 队列已满，返回false
            }
            if (back)
            {
                m_deque.push_back(value);
            }
            else
            {
                m_deque.push_front(value);
            }
            m_cond.notify_all();
            return true;
        }
    }

    // 通用出队操作
    bool Dequeue(T& value, bool back)
    {
        if constexpr (IS_BLOCKING)
        {
            std::unique_lock lock(m_mtx);
            if (!m_cond.wait_for(
                    lock, m_timeoutMs, [this] { return !m_deque.empty() || !m_isRunning; }))
            {
                return false; // 超时，返回false
            }
            if (!m_deque.empty())
            {
                value = back ? std::move(m_deque.back()) : std::move(m_deque.front());
                if (back)
                {
                    m_deque.pop_back();
                }
                else
                {
                    m_deque.pop_front();
                }
                m_cond.notify_all();
                return true;
            }
            return false;
        }
        else
        {
            std::lock_guard lock(m_mtx);
            if (m_deque.empty())
            {
                return false; // 队列为空，返回false
            }
            value = back ? std::move(m_deque.back()) : std::move(m_deque.front());
            if (back)
            {
                m_deque.pop_back();
            }
            else
            {
                m_deque.pop_front();
            }
            m_cond.notify_all();
            return true;
        }
    }
};

template <typename T>
using BlockingDeque = OptionalBlockingDeque<T, true>;

template <typename T>
using NonBlockingDeque = OptionalBlockingDeque<T, false>;

#endif // DOUBLE_ENDED_BLOCKING_QUEUE_H
