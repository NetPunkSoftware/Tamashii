#include "synchronization/condition_variable.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	void condition_variable::notify_one() noexcept
	{
		notify_one(detail::fiber_pool_instance);
	}

	void condition_variable::notify_one(fiber_pool_base* fiber_pool) noexcept
	{
		assert(fiber_pool != nullptr && "Conditions variable require a fiber pool");

		np::fiber_base* fiber;
		if (_waiting_fibers.try_dequeue(fiber))
		{
			fiber_pool->unblock({}, fiber);
		}
	}

	void condition_variable::notify_all() noexcept
	{
		notify_all(detail::fiber_pool_instance);
	}

	void condition_variable::notify_all(fiber_pool_base* fiber_pool) noexcept
	{
		assert(fiber_pool != nullptr && "Conditions variable require a fiber pool");

		np::fiber_base* fiber;
		while (_waiting_fibers.try_dequeue(fiber))
		{
			fiber_pool->unblock({}, fiber);
		}
	}

	void condition_variable::wait(np::mutex& mutex) noexcept
	{
		wait(detail::fiber_pool_instance, mutex);
	}

	void condition_variable::wait(fiber_pool_base* fiber_pool, np::mutex& mutex) noexcept
	{
		assert(fiber_pool != nullptr && "Conditions variable require a fiber pool");

		_waiting_fibers.enqueue(fiber_pool->this_fiber());
		mutex.unlock();
		fiber_pool->block({});
		mutex.lock();
	}
}
