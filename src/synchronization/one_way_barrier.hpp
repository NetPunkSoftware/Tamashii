#pragma once

#include "synchronization/mutex.hpp"

#include <atomic>


namespace np
{
	class one_way_barrier
	{
	public:
		one_way_barrier(std::size_t size) noexcept;

		void reset(std::size_t size) noexcept;
		void decrease() noexcept;
		void wait() noexcept;

	private:
		std::atomic<std::size_t> _size;
		np::mutex _mutex;
	};
}
