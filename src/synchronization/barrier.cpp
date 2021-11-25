#include "synchronization/barrier.hpp"

#include "pool/fiber_pool.hpp"


namespace np
{
	barrier::barrier(std::size_t size) noexcept :
		_size(size),
		_waiting(size)
	{}

	void barrier::reset(std::size_t size) noexcept
	{
		_size = size;
		_waiting = size;
	}

	void barrier::wait() noexcept
	{
		if (--_waiting == 0)
		{
			while (_waiting_fibers.size_approx() != _size)
			{
				detail::fiber_pool_instance->yield();
			}

			np::fiber* fiber;
			while (_waiting_fibers.try_dequeue(fiber))
			{
				assert(detail::fiber_pool_instance != nullptr && "Mutexes with waiting fibers require a fiber pool");
				detail::fiber_pool_instance->unblock({}, fiber);
			}

			return;
		}
		
		_waiting_fibers.enqueue(detail::fiber_pool_instance->this_fiber());
		detail::fiber_pool_instance->block({});
	}
}
