#include "synchronization/mutex.hpp"

#include "pool/fiber_pool.hpp"

#include <palanteer.h>


namespace np
{
    mutex::mutex() noexcept :
        _status(status::unlocked)
    {}

    void mutex::lock() noexcept
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

            auto fiber = this_fiber::instance();
            assert(fiber && fiber->get_fiber_pool() != nullptr && "Fiber that block in mutexes must come from fiber pools");

            _waiting_fibers.enqueue(fiber);
            //_status.store(status::locked, std::memory_order_release);
            _status.store(status::locked);
            fiber->get_fiber_pool()->block({});
        }
    }

    bool mutex::try_lock() noexcept
    {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while(!try_lock())
        return _status.load(std::memory_order_relaxed) == status::unlocked &&
            _status.exchange(status::locked, std::memory_order_acquire) == status::unlocked;
    }

    void mutex::unlock() noexcept
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
        np::fiber_base* fiber;
        while (_waiting_fibers.try_dequeue(fiber))
        {
            assert(fiber && fiber->get_fiber_pool() != nullptr && "Mutexes with waiting fibers require a fiber pool");
            fiber->get_fiber_pool()->unblock({}, fiber);
        }

        // Unlock now
        _status.store(status::unlocked, std::memory_order_release);
    }
}
