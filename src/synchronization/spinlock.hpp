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
	class spinlock
	{
    public:
        spinlock() :
            _lock(false)
        {}

        template <typename rep, typename period>
        bool lock(std::chrono::duration<rep, period> duration) noexcept 
        {
            auto target_spin_locktimepoint = std::chrono::steady_clock::now() + duration;

            for (;;) 
            {
                // Optimistically assume the lock is free on the first try
                if (!_lock.exchange(true, std::memory_order_acquire)) 
                {
                    return true;
                }

                // Wait for lock to be released without generating cache misses
                while (_lock.load(std::memory_order_relaxed)) 
                {
                    if (std::chrono::steady_clock::now() >= target_spin_locktimepoint)
                    {
                        return false;
                    }

#if defined(NETPUNK_SPINLOCK_PAUSE)
#if defined(_MSC_VER)
                    _mm_pause();
#else
                    __builtin_ia32_pause();
#endif
#endif // NETPUNK_SPINLOCK_PAUSE
                }
            }
        }

        bool lock() noexcept
        {
            return lock(std::chrono::milliseconds(100));
        }

        bool try_lock() noexcept 
        {
            // First do a relaxed load to check if lock is free in order to prevent
            // unnecessary cache misses if someone does while(!try_lock())
            return !_lock.load(std::memory_order_relaxed) &&
                !_lock.exchange(true, std::memory_order_acquire);
        }

        void unlock() noexcept 
        {
            assert(_lock.load(std::memory_order_relaxed) && "Can't unlock a non-locked mutex");

            // Unlock now
            _lock.store(false, std::memory_order_release);
        }

    private:
        std::atomic<bool> _lock;
        moodycamel::ConcurrentQueue<np::fiber*> _waiting_fibers;
	};
}
