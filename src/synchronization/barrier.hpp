#pragma once

#include "synchronization/mutex.hpp"

#include <atomic>


namespace np
{
	class fiber_pool_base;

	class barrier
	{
	public:
		barrier(std::size_t size) noexcept;

		void reset(std::size_t size) noexcept;
		void wait() noexcept;

	private:
		std::size_t _size;
		std::atomic<std::size_t> _waiting;
		moodycamel::ConcurrentQueue<np::fiber_base*> _waiting_fibers;
	};
}
