// @Description: ThreadPool Impl support EnqueMsg() and WaitSync()

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

#include "CustomLog.h"

class ThreadPool
{
public:
    explicit inline ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    inline ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = default;
    ThreadPool& operator=(ThreadPool&&) = default;

    template <class F, class... Args>
    auto EnqueMsg(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    template <typename T>
    inline void WaitSync(std::future<T>& future);

    inline void WaitAll();
    inline size_t GetThreadCount() const noexcept;
    inline size_t GetPendingTaskCount() const;
    inline size_t GetActiveTaskCount() const;

private:
    inline void WorkerThread();
    inline void Shutdown();

private:
    std::vector<std::thread> workers_{};
    std::queue<std::function<void()>> tasks_{};

    mutable std::mutex queueMutex_;
    std::condition_variable cond_;
    std::condition_variable complete_;

    std::atomic<bool> stop_{};
    std::atomic<size_t> activeTasks_{};
};

// 实现
inline ThreadPool::ThreadPool(size_t numThreads)
{
    if (numThreads == 0)
    {
        numThreads = std::thread::hardware_concurrency();
    }

    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i)
    {
        workers_.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

inline ThreadPool::~ThreadPool()
{
    Shutdown();
}

template <class F, class... Args>
auto ThreadPool::EnqueMsg(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
{

    using R = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<R()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<R> result = task->get_future();

    {
        std::unique_lock lock(queueMutex_);

        if (stop_)
        {
            // 处理日志
            // PV("error, call while threadPool is stopped");
            return std::future<R>{};
        }

        tasks_.emplace([task]() { (*task)(); });
    }

    cond_.notify_one();
    return result;
}

template <typename T>
inline void ThreadPool::WaitSync(std::future<T>& future)
{
    future.wait();
}

inline void ThreadPool::WaitAll()
{
    std::unique_lock lock(queueMutex_);
    complete_.wait(lock, [this]() { return tasks_.empty() && activeTasks_ == 0; });
}

inline size_t ThreadPool::GetThreadCount() const noexcept
{
    return workers_.size();
}

inline size_t ThreadPool::GetPendingTaskCount() const
{
    std::unique_lock lock(queueMutex_);
    return tasks_.size();
}

inline size_t ThreadPool::GetActiveTaskCount() const
{
    return activeTasks_;
}

inline void ThreadPool::WorkerThread()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock lock(queueMutex_);
            cond_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty())
            {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
            ++activeTasks_;
        }

        task();

        {
            std::lock_guard lock(queueMutex_);
            --activeTasks_;
        }

        complete_.notify_all();
    }
}

inline void ThreadPool::Shutdown()
{
    {
        std::unique_lock lock(queueMutex_);
        stop_ = true;
    }

    cond_.notify_all();

    for (std::thread& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

#endif // THREADPOOL_H