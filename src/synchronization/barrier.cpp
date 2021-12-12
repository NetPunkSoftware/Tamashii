#include "synchronization/barrier.hpp"

#include "pool/fiber_pool.hpp"


namespace np
{
	barrier::barrier(std::size_t size) noexcept :
		_size(size),
		_waiting(size),
		_waiting_fibers()
	{}

	void barrier::reset(std::size_t size) noexcept
	{
		assert(_waiting_fibers.size_approx() == 0 && "Can not reset while there are waiting fibers");
		_size = size;
		_waiting = size;
	}

	void barrier::wait() noexcept
	{
		wait(detail::fiber_pool_instance);
	}

	void barrier::wait(fiber_pool_base* fiber_pool) noexcept
	{
		if (--_waiting == 0)
		{
			while (_waiting_fibers.size_approx() != _size)
			{
				fiber_pool->yield();
			}

			np::fiber_base* fiber;
			while (_waiting_fibers.try_dequeue(fiber))
			{
				assert(fiber_pool != nullptr && "Mutexes with waiting fibers require a fiber pool");
				fiber_pool->unblock({}, fiber);
			}

			return;
		}
		
		_waiting_fibers.enqueue(fiber_pool->this_fiber());
		fiber_pool->block({});
	}
}
