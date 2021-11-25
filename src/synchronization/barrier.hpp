#pragma once

#include "synchronization/mutex.hpp"

#include <atomic>


namespace np
{
	class barrier
	{
	public:
		barrier(std::size_t size) noexcept;

		void reset(std::size_t size) noexcept;
		void wait() noexcept;

	private:
		std::size_t _size;
		std::atomic<std::size_t> _waiting;
		moodycamel::ConcurrentQueue<np::fiber*> _waiting_fibers;
	};
}
