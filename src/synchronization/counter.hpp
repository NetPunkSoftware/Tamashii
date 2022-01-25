#pragma once

#include "utils/badge.hpp"

#include <atomic>
#include <inplace_function.h>


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

        inline void increase(badge<fiber_pool_base>) noexcept;
		inline void done(badge<fiber_base>, fiber_pool_base* fiber_pool) noexcept;

#if !defined(NDEBUG)
		template <typename C>
		inline void on_wait_done(C&& callback) noexcept;
#endif

    protected:
        void done_impl(fiber_pool_base* fiber_pool) noexcept;

	private:
		bool _ignore_waiter;
		std::atomic<std::size_t> _size;
		std::atomic<std::size_t> _done;
		std::atomic<fiber_base*> _fiber;

#if !defined(NDEBUG)
		stdext::inplace_function<void()> _on_wait_end;
#endif
	};


    inline void counter::increase(badge<fiber_pool_base>) noexcept
    {
        ++_size;
    }

    inline void counter::done(badge<fiber_base>, fiber_pool_base* fiber_pool) noexcept
    {
		done_impl(fiber_pool);
    }

#if !defined(NDEBUG)
	template <typename C>
	inline void counter::on_wait_done(C&& callback) noexcept
	{
		_on_wait_end = std::forward<C>(callback);
	}
#endif

}
