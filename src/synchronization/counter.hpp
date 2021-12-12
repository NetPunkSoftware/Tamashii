#pragma once

#include "utils/badge.hpp"

#include <atomic>


namespace np
{
	class fiber_base;
    class fiber_pool_base;

	class counter
	{
	public:
		counter() noexcept;
		counter(bool ignore_waiter) noexcept;

		void reset() noexcept;
		void wait() noexcept;

		// Explicit methods
		void wait(fiber_pool_base* fiber_pool) noexcept;

        inline void increase(badge<fiber_pool_base>) noexcept;
		inline void done(badge<fiber_base>, fiber_pool_base* fiber_pool) noexcept;

    protected:
        void done_impl(fiber_pool_base* fiber_pool) noexcept;

	private:
		bool _ignore_waiter;
		std::atomic<std::size_t> _size;
		std::atomic<std::size_t> _done;
		std::atomic<fiber_base*> _fiber;
	};


    inline void counter::increase(badge<fiber_pool_base>) noexcept
    {
        ++_size;
    }

    inline void counter::done(badge<fiber_base>, fiber_pool_base* fiber_pool) noexcept
    {
		done_impl(fiber_pool);
    }
}
