#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>

template <class T>
class shared
{
public:
    template <class F>
    typename std::result_of<F(T &)>::type use(F &&f)
    {
        std::lock_guard<std::mutex> guard(lock);

        return f(value);
    }

    void notify_one()
    {
        cv.notify_one();
    }

    void notify_all()
    {
        cv.notify_all();
    }

    template <class F>
    typename std::result_of<F(T &)>::type wait(F &&f, std::function<bool(const T &)> continue_predicate, std::atomic_bool &exit_flag)
    {
        std::unique_lock<std::mutex> wait_lock(lock);

        while (continue_predicate(value))
        {
            if (exit_flag)
            {
                break;
            }
            cv.wait(wait_lock);
        }

        if (wait_lock.owns_lock() && !exit_flag)
        {
            return f(value);
        }
    }

private:
    T value;
    std::mutex lock;
    std::condition_variable cv;
};