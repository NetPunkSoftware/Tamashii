#include "synchronization/event.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	void event::notify() noexcept
	{
		assert(this_fiber::fiber_pool() != nullptr && "Conditions variable require a fiber pool");

		if (_awaiter)
		{
			this_fiber::fiber_pool()->unblock({}, _awaiter);
		}
	}

	void event::wait(np::mutex& mutex) noexcept
	{
		assert(this_fiber::fiber_pool() != nullptr && "Conditions variable require a fiber pool");

		_awaiter = this_fiber::instance();
		mutex.unlock();
		_awaiter->get_fiber_pool()->block({});
		mutex.lock();
	}
}
