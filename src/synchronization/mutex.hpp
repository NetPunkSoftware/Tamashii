#pragma once

#include "utils/badge.hpp"

#include <atomic>
#include <chrono>

#include <concurrentqueue.h>


namespace np
{
    class barrier;
    class fiber;
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

        void lock() noexcept;
        bool try_lock() noexcept;
        void unlock() noexcept;

    private:
        std::atomic<status> _status;
        moodycamel::ConcurrentQueue<np::fiber*> _waiting_fibers;
    };
}
