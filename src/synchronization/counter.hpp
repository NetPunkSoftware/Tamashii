#pragma once

#include "utils/badge.hpp"

#include <atomic>


namespace np
{
	class fiber_base;
	template <typename traits> class fiber;
    template <typename traits> class fiber_pool;

	class counter
	{
	public:
		counter() noexcept;
		counter(bool ignore_waiter) noexcept;

		void reset() noexcept;
		void wait() noexcept;

        template <typename traits>
        void increase(badge<fiber_pool<traits>>) noexcept;

		template <typename traits>
		inline void done(badge<np::fiber<traits>>) noexcept;

    protected:
        void done_impl() noexcept;

	private:
		bool _ignore_waiter;
		std::atomic<std::size_t> _size;
		std::atomic<std::size_t> _done;
		std::atomic<fiber_base*> _fiber;
	};


    template <typename traits>
    void counter::increase(badge<fiber_pool<traits>>) noexcept
    {
        ++_size;
    }

	template <typename traits>
    inline void counter::done(badge<np::fiber<traits>>) noexcept
    {
		done_impl();
    }
}
