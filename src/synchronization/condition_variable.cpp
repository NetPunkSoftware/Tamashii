#include "synchronization/condition_variable.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	void condition_variable::notify_one() noexcept
	{
		assert(detail::fiber_pool_instance != nullptr && "Conditions variable require a fiber pool");

		np::fiber_base* fiber;
		if (_waiting_fibers.try_dequeue(fiber))
		{
			np::detail::fiber_pool_instance->unblock({}, fiber);
		}
	}

	void condition_variable::notify_all() noexcept
	{
		assert(detail::fiber_pool_instance != nullptr && "Conditions variable require a fiber pool");

		np::fiber_base* fiber;
		while (_waiting_fibers.try_dequeue(fiber))
		{
			np::detail::fiber_pool_instance->unblock({}, fiber);
		}
	}

	void condition_variable::wait(np::mutex& mutex) noexcept
	{
		assert(detail::fiber_pool_instance != nullptr && "Conditions variable require a fiber pool");

		_waiting_fibers.enqueue(np::detail::fiber_pool_instance->this_fiber());
		mutex.unlock();
		np::detail::fiber_pool_instance->block({});
		mutex.lock();
	}
}
