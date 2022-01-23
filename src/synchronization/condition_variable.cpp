#include "synchronization/condition_variable.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	void condition_variable::notify_one() noexcept
	{
		np::fiber_base* fiber;
		if (_waiting_fibers.try_dequeue(fiber))
		{
			assert(fiber->get_fiber_pool() != nullptr && "Conditions variable require a fiber pool");
			fiber->get_fiber_pool()->unblock({}, fiber);
		}
	}

	void condition_variable::notify_all() noexcept
	{
		np::fiber_base* fiber;
		while (_waiting_fibers.try_dequeue(fiber))
		{
			assert(fiber->get_fiber_pool() != nullptr && "Conditions variable require a fiber pool");
			fiber->get_fiber_pool()->unblock({}, fiber);
		}
	}

	void condition_variable::wait(np::mutex& mutex) noexcept
	{
		auto fiber = this_fiber::instance();
		assert(fiber->get_fiber_pool() != nullptr && "Conditions variable require a fiber pool");

		_waiting_fibers.enqueue(fiber);
		mutex.unlock();
		fiber->get_fiber_pool()->block({});
		mutex.lock();
	}
}
