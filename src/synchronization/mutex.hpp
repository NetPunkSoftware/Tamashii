#pragma once

#include "utils/badge.hpp"

#include <atomic>
#include <chrono>

#include <concurrentqueue.h>


namespace np
{
    class barrier;
    class fiber_base;
    class fiber_pool_base;
}

namespace np
{
    class mutex
    {
        enum class status
        {
            unlocked,
            unlocking,
            locked,
            adding_to_waitlist
        };

    public:
        mutex() noexcept;

        mutex(mutex&& other) noexcept;
        mutex& operator=(mutex&& other) noexcept;

        void lock() noexcept;
        bool try_lock() noexcept;
        void unlock() noexcept;

    private:
        std::atomic<status> _status;
        moodycamel::ConcurrentQueue<np::fiber_base*> _waiting_fibers;
    };
}
