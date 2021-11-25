#pragma once

#include "utils/badge.hpp"

#include <atomic>


namespace np
{
	class fiber;
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

		inline void done(badge<fiber>) noexcept;

    protected:
        void done() noexcept;

	private:
		bool _ignore_waiter;
		std::atomic<std::size_t> _size;
		std::atomic<std::size_t> _done;
		std::atomic<np::fiber*> _fiber;
	};


    template <typename traits>
    void counter::increase(badge<fiber_pool<traits>>) noexcept
    {
        ++_size;
    }
    
    inline void counter::done(badge<fiber>) noexcept
    {
        done();
    }
}
