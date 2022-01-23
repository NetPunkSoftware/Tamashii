#include "synchronization/counter.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	counter::counter() noexcept :
		counter { false }
	{}

	counter::counter(bool ignore_waiter) noexcept :
		_ignore_waiter(ignore_waiter),
        _size(0),
		_done(0),
		_fiber(nullptr)
	{}

	void counter::reset() noexcept
	{
		_size = 0;
        _done = 0;
		_fiber = nullptr;
	}

	void counter::done_impl(fiber_pool_base* fiber_pool) noexcept
	{
        if (_ignore_waiter)
        {
            ++_done;
            return;
        }

        np::fiber_base* waiter;
        while (!(waiter = _fiber.load(std::memory_order_relaxed)))
        {
			fiber_pool->yield();
		}

		if (++_done == _size)
		{
			fiber_pool->unblock({}, waiter);
		}
	}

	void counter::wait() noexcept
	{
		assert(_fiber.load(std::memory_order_relaxed) == nullptr && "Only one fiber can wait on a counter");

		auto fiber = this_fiber::instance();
		_fiber.store(fiber, std::memory_order_release);
		fiber->get_fiber_pool()->block({});
	}
}
