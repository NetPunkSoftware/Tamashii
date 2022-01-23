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
		if (--_waiting == 0)
		{
			while (_waiting_fibers.size_approx() != _size)
			{
				this_fiber::yield();
			}

			np::fiber_base* fiber;
			while (_waiting_fibers.try_dequeue(fiber))
			{
				assert(fiber->get_fiber_pool() != nullptr && "Mutexes with waiting fibers require a fiber pool");
				fiber->get_fiber_pool()->unblock({}, fiber);
			}

			return;
		}
		
		auto fiber = this_fiber::instance();
		_waiting_fibers.enqueue(fiber);
		fiber->get_fiber_pool()->block({});
	}
}
