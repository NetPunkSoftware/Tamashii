#pragma once

#include <atomic>
#include <synchronization/mutex.hpp>


namespace np
{
	class one_way_barrier
	{
	public:
		one_way_barrier(std::size_t size) noexcept :
			_size(size)
		{
			_mutex.lock();
		}

		void reset(std::size_t size) noexcept
		{
			_size = size;
			_mutex.lock();
		}

		void decrease()
		{
			if (--_size == 0)
			{
				_mutex.unlock();
			}
		}

		void wait()
		{
			_mutex.lock(); // This suspends the thread until another one calls unlock on the barrier
			_mutex.unlock(); 
		}

	private:
		std::atomic<std::size_t> _size;
		np::mutex _mutex;
	};
}
