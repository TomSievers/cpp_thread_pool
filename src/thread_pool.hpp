#pragma once
#include <future>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include "future.hpp"
#include "shared.hpp"

class thread_pool
{
public:
    explicit thread_pool(size_t worker_count = std::thread::hardware_concurrency());
    ~thread_pool();
    template <class F, class... Args>
    std::future<typename std::result_of<F(Args...)>::type> enqueue(F &&f, Args &&...args);
    template <class F>
    cancellable_future<typename std::result_of<F(std::shared_ptr<cancelation_token>)>::type> enqueue_with_cancellation(F &&f);

    template <class R>
    void _enqueue(std::shared_ptr<std::function<R()>> task, std::shared_ptr<std::promise<R>> promise)
    {
        auto bound_task = [task, promise]()
        {
            try
            {
                if constexpr (std::is_same<R, void>::value)
                {
                    (*task)();
                    promise->set_value();
                }
                else
                {
                    promise->set_value((*task)());
                }
            }
            catch (...)
            {
                promise->set_exception(std::current_exception());
            }
        };
        tasks.use([bound_task](std::queue<std::function<void()>> &tasks)
                  { tasks.emplace(bound_task); });
        tasks.notify_one();
    }

    void worker();
    thread_pool(const thread_pool &) = delete;
    thread_pool(thread_pool &&) = delete;
    thread_pool &operator=(thread_pool &&) = delete;
    thread_pool &operator=(const thread_pool &) = delete;

private:
    void stop_threads() noexcept;
    std::vector<std::thread> workers;
    shared<std::queue<std::function<void()>>> tasks;
    std::atomic_bool stopped = false;
};

template <class F, class... Args>
std::future<typename std::result_of<F(Args...)>::type> thread_pool::enqueue(F &&f, Args &&...args)
{
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::function<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto promise = std::make_shared<std::promise<return_type>>();

    auto future = promise->get_future();

    _enqueue(task, promise);

    return future;
}

template <class F>
cancellable_future<typename std::result_of<F(std::shared_ptr<cancelation_token>)>::type> thread_pool::enqueue_with_cancellation(F &&f)
{
    using return_type = typename std::result_of<F(std::shared_ptr<cancelation_token>)>::type;
    auto token = std::make_shared<cancelation_token>();

    auto promise = std::make_shared<std::promise<return_type>>();

    auto future = cancellable_future(promise->get_future());

    auto task = std::make_shared<std::function<return_type()>>(std::bind(std::forward<F>(f), future.token()));

    _enqueue(task, promise);

    return future;
}

void thread_pool::worker()
{
    while (true)
    {
        std::function<void()> task;

        {
            auto continue_predicate = [](const std::queue<std::function<void()>> tasks)
            {
                return tasks.empty();
            };

            auto task_get = [&task](std::queue<std::function<void()>> &tasks)
            {
                task = tasks.front();
                tasks.pop();
            };

            tasks.wait(task_get, continue_predicate, stopped);

            if (stopped)
            {
                return;
            }
        }

        task();
    }
}

thread_pool::thread_pool(size_t worker_count)
{
    try
    {
        workers.reserve(worker_count);

        for (size_t worker = 0; worker < worker_count; ++worker)
        {
            workers.emplace_back(&thread_pool::worker, this);
        }
    }
    catch (...)
    {
        stop_threads();
        throw;
    }
}

thread_pool::~thread_pool()
{
    stop_threads();
}

void thread_pool::stop_threads() noexcept
{
    stopped.store(true);
    tasks.notify_all();

    for (auto &worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}