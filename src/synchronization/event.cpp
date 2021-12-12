#include "synchronization/event.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	void event::notify() noexcept
	{
		notify(detail::fiber_pool_instance);
	}

	void event::notify(fiber_pool_base* fiber_pool) noexcept
	{
		assert(fiber_pool != nullptr && "Conditions variable require a fiber pool");

		if (_awaiter)
		{
			fiber_pool->unblock({}, _awaiter);
		}
	}

	void event::wait(np::mutex& mutex) noexcept
	{
		wait(detail::fiber_pool_instance, mutex);
	}

	void event::wait(fiber_pool_base* fiber_pool, np::mutex& mutex) noexcept
	{
		assert(fiber_pool != nullptr && "Conditions variable require a fiber pool");

		_awaiter = fiber_pool->this_fiber();
		mutex.unlock();
		fiber_pool->block({});
		mutex.lock();
	}
}
