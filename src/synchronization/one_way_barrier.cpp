#include "synchronization/one_way_barrier.hpp"


namespace np
{
	one_way_barrier::one_way_barrier(std::size_t size) noexcept :
		_size(size)
	{
		_mutex.lock();
	}

	void one_way_barrier::reset(std::size_t size) noexcept
	{
		_size = size;
		_mutex.lock();
	}

	void one_way_barrier::decrease() noexcept
	{
		if (--_size == 0)
		{
			_mutex.unlock();
		}
	}

	void one_way_barrier::wait() noexcept
	{
		_mutex.lock(); // This suspends the thread until another one calls unlock on the barrier
		_mutex.unlock();
	}
}
