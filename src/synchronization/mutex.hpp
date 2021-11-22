#pragma once

#include "pool/fiber_pool.hpp"

#include <atomic>
#include <chrono>

#include <concurrentqueue.h>


namespace np
{
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
        mutex() :
            _status(status::unlocked)
        {}

        void lock() noexcept
        {
            for (;;)
            {
                status expect = status::locked;
                if (!_status.compare_exchange_strong(expect, status::adding_to_waitlist))
                {
                    if (expect == status::unlocked)
                    {
                        if (_status.compare_exchange_strong(expect, status::locked))
                        {
                            return;
                        }
                    }

#if defined(NETPUNK_SPINLOCK_PAUSE)
#if defined(_MSC_VER)
                    _mm_pause();
#else
                    __builtin_ia32_pause();
#endif
#endif // NETPUNK_SPINLOCK_PAUSE

                    continue;
                }

                assert(detail::fiber_pool_instance != nullptr && "Mutexes that end spinlock periods require a fiber pool");

                _waiting_fibers.enqueue(detail::fiber_pool_instance->this_fiber());
                //_status.store(status::locked, std::memory_order_release);
                _status.store(status::locked);
                detail::fiber_pool_instance->block({});
            }
        }

        bool try_lock() noexcept
        {
            // First do a relaxed load to check if lock is free in order to prevent
            // unnecessary cache misses if someone does while(!try_lock())
            return _status.load(std::memory_order_relaxed) == status::unlocked &&
                _status.exchange(status::locked, std::memory_order_acquire) == status::unlocked;
        }

        void unlock() noexcept
        {
            status expect_locked = status::locked;
            while (!_status.compare_exchange_strong(expect_locked, status::unlocking))
            {
                expect_locked = status::locked;

#if defined(NETPUNK_SPINLOCK_PAUSE)
#if defined(_MSC_VER)
                _mm_pause();
#else
                __builtin_ia32_pause();
#endif
#endif // NETPUNK_SPINLOCK_PAUSE
            }

            // Unblock all waiting fibers
            np::fiber* fiber;
            while (_waiting_fibers.try_dequeue(fiber))
            {
                assert(detail::fiber_pool_instance != nullptr && "Mutexes with waiting fibers require a fiber pool");
                detail::fiber_pool_instance->unblock({}, fiber);
            }

            // Unlock now
            _status.store(status::unlocked, std::memory_order_release);
        }

    private:
        std::atomic<status> _status;
        moodycamel::ConcurrentQueue<np::fiber*> _waiting_fibers;
    };
}
