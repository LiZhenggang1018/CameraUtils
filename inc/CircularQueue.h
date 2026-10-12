// @Description: simple thread-safe circular queue impl using ring buffer

#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <thread>

#include "CustomLog.h"

// T:元素类型; Block:队列阻塞行为模板参数
template <typename T, bool Block>
class CircularQueue
{
public:
    explicit CircularQueue(size_t capacity) : capacity_(capacity), buffer_(new T[capacity])
    {
    }

    ~CircularQueue()
    {
        delete[] buffer_;
    }

    bool Empty()
    {
        std::unique_lock lock(mutex_);
        return size_ == 0;
    }

    bool Full()
    {
        std::unique_lock lock(mutex_);
        return size_ == capacity_;
    }

    size_t Size()
    {
        std::unique_lock lock(mutex_);
        return size_;
    }

    size_t Capacity()
    {
        return capacity_;
    }

    // 覆盖最旧元素的Push实现
    bool Push(const T& value)
    {
        std::unique_lock lock(mutex_);
        if (size_ == capacity_)
        {
            // 队列已满，覆盖最旧元素
            buffer_[tail_] = value;
            tail_ = (tail_ + 1) % capacity_;
            head_ = (head_ + 1) % capacity_; // 头部也前进，保持队列满状态
            // size_ 保持不变，因为覆盖了一个元素
            not_empty_.notify_one();
            return true;
        }

        // 非覆盖模式下的正常阻塞队列Push逻辑
        // if constexpr (Block) {
        //    while (size_ == capacity_) {
        //        not_full_.wait(lock);
        //    }
        //}
        // else {
        //    if (size_ == capacity_) {
        //        return false;
        //    }
        //}

        // 队列未满，正常Push
        buffer_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
        not_empty_.notify_one();
        return true;
    }

    // Push的右值引用版本
    bool Push(T&& value)
    {
        std::unique_lock lock(mutex_);
        if (size_ == capacity_)
        {
            buffer_[tail_] = std::move(value);
            tail_ = (tail_ + 1) % capacity_;
            head_ = (head_ + 1) % capacity_;
            not_empty_.notify_one();
            return true;
        }

        // if constexpr (Block) {
        //     while (size_ == capacity_) {
        //         not_full_.wait(lock);
        //     }
        // }
        // else {
        //     if (size_ == capacity_) {
        //         return false;
        //     }
        // }

        buffer_[tail_] = std::move(value);
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
        not_empty_.notify_one();
        return true;
    }

    bool Pop(T& value)
    {
        std::unique_lock lock(mutex_);
        if constexpr (Block)
        {
            while (size_ == 0)
            {
                not_empty_.wait(lock);
            }
        }
        else
        {
            if (size_ == 0)
            {
                return false;
            }
        }
        value = std::move(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        --size_;
        not_full_.notify_one();
        return true;
    }

    // 查看但不移除头部元素
    bool Front(T& value)
    {
        std::unique_lock lock(mutex_);
        if (size_ == 0)
        {
            return false;
        }
        value = buffer_[head_];
        return true;
    }

    // 查看但不移除尾部元素
    bool Back(T& value)
    {
        std::unique_lock lock(mutex_);
        if (size_ == 0)
        {
            return false;
        }
        value = buffer_[(tail_ + capacity_ - 1) % capacity_];
        return true;
    }

    // 清空队列
    void Clear()
    {
        std::unique_lock lock(mutex_);
        head_ = 0;
        tail_ = 0;
        size_ = 0;
        not_full_.notify_all();
    }

private:
    const size_t capacity_;
    size_t size_{};
    size_t head_{};
    size_t tail_{};
    T* buffer_{};
    std::mutex mutex_{};
    std::condition_variable not_full_{};
    std::condition_variable not_empty_{};
};

// 阻塞式，队列空时Pop()会阻塞等待
template <typename T>
using BlockingCircularQueue = CircularQueue<T, true>;

// 非阻塞式，队列空时Pop()会立即返回失败
template <typename T>
using NonBlockingCircularQueue = CircularQueue<T, false>;

#endif // CIRCULAR_QUEUE_H