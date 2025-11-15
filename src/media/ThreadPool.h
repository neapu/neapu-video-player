#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <future>
#include <functional>
#include <queue>

namespace media {
class ThreadPool {
public:
    explicit ThreadPool(size_t threads)
    {
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_queueMutex);
                        m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                        if (m_stop && m_tasks.empty()) {
                            return;
                        }
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    task();
                }
            });
        }
    }
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for (auto& worker : m_workers) {
            worker.join();
        }
    }

    template <class F, class... Args> auto push_task(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
    {
        using return_type = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_tasks.emplace([task]() { (*task)(); });
        }
        m_condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::condition_variable m_condition;
    std::mutex m_queueMutex;
    bool m_stop{false};
};

class ThreadPoolManager {
public:
    static ThreadPoolManager& instance()
    {
        static ThreadPoolManager instance;
        return instance;
    }

    void init(size_t threads = 0)
    {
        if (m_threadPool) {
            return;
        }
        if (threads == 0) {
            threads = std::thread::hardware_concurrency();
        }
        m_threadPool = std::make_unique<ThreadPool>(threads);
    }

    ThreadPool& threadPool() const { return *m_threadPool; }

private:
    ThreadPoolManager() = default;
    ThreadPoolManager(const ThreadPoolManager&) = delete;
    ThreadPoolManager& operator=(const ThreadPoolManager&) = delete;
    ThreadPoolManager(ThreadPoolManager&&) = delete;

    std::unique_ptr<ThreadPool> m_threadPool{nullptr};
};
} // namespace core

#endif // THREADPOOL_H