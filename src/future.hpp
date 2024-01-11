#pragma once

#include <future>
#include <utility>

using namespace std::chrono_literals;

class cancelation_token
{
public:
    explicit cancelation_token()
        : _cancel(false)
    {
    }
    cancelation_token(const cancelation_token &) = delete;
    cancelation_token(cancelation_token &&rhs) = delete;

    bool cancelled() noexcept
    {
        return _cancel.load();
    }

    void cancel() noexcept
    {
        _cancel.store(true);
        _wait.notify_all();
    }

    template <class Rep, class Period>
    void sleep_for(const std::chrono::duration<Rep, Period> &duration)
    {
        struct fake_lock
        {
            void lock() {}
            void unlock() {}
        } lock;

        auto end = std::chrono::steady_clock::now() + duration;

        while (!cancelled())
        {
            if (_wait.wait_until(lock, end) == std::cv_status::timeout)
            {
                return;
            }
        }
    }

private:
    std::atomic_bool _cancel = false;
    std::condition_variable_any _wait;
};

template <class T>
class cancellable_future
{
public:
    cancellable_future(std::future<T> &&future)
        : future(std::move(future)),
          _token(std::make_shared<cancelation_token>())
    {
    }
    cancellable_future(cancellable_future &&rhs) noexcept
        : future(std::move(rhs.future)),
          _token(std::exchange(rhs._token, nullptr))
    {
    }
    cancellable_future(const cancellable_future &) = delete;

    T get()
    {
        return future.get();
    }

    bool valid() const noexcept
    {
        return future.valid();
    }

    void wait() const
    {
        future.wait();
    }

    template <class Rep, class Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period> &timeout_duration) const
    {
        return future.wait_for(timeout_duration);
    }

    template <class Clock, class Duration>
    std::future_status wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time) const
    {
        return future.wait_until(timeout_time);
    }

    void cancel() noexcept
    {
        _token->cancel();
    }

    std::shared_ptr<cancelation_token> token()
    {
        return _token;
    }

private:
    std::future<T> future;
    std::shared_ptr<cancelation_token> _token;
};