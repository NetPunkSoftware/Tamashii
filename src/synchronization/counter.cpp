#include "synchronization/counter.hpp"

#include "pool/fiber_pool.hpp"


namespace np
{
	counter::counter() noexcept :
		_waiting(false),
		_total(0),
		_done(0)
	{
		_mutex.lock();
	}

	void counter::reset() noexcept
	{
		_waiting = false;
		_total = 0;
		_done = 0;
		_mutex.lock();
	}

	void counter::done(badge<fiber>) noexcept
	{
		// Wait until someone is waitingn at the counter
		while (!_waiting.load(std::memory_order_relaxed))
		{
			detail::fiber_pool_instance->yield();
		}

		if (++_done == _total)
		{
			_mutex.unlock();
		}
	}

	void counter::wait() noexcept
	{
		_waiting.store(true, std::memory_order_release);
		_mutex.lock(); // This suspends the thread until another one calls unlock on the barrier
		_mutex.unlock();
	}
}
