#include "synchronization/event.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	void event::notify() noexcept
	{
		assert(detail::fiber_pool_instance != nullptr && "Conditions variable require a fiber pool");

		if (_awaiter)
		{
			np::detail::fiber_pool_instance->unblock({}, _awaiter);
		}
	}

	void event::wait(np::mutex& mutex) noexcept
	{
		assert(detail::fiber_pool_instance != nullptr && "Conditions variable require a fiber pool");

		_awaiter = np::detail::fiber_pool_instance->this_fiber();
		mutex.unlock();
		np::detail::fiber_pool_instance->block({});
		mutex.lock();
	}
}
