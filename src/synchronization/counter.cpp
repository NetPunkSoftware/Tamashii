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
#if !defined(NDEBUG)
		,_on_wait_end([] {})
#endif
	{}

	counter::counter(counter&& other) noexcept :
		_ignore_waiter(other._ignore_waiter),
		_size((std::size_t)other._size),
		_done((std::size_t)other._done),
		_fiber((fiber_base*)other._fiber)
#if !defined(NDEBUG)
		, _on_wait_end(std::move(other._on_wait_end))
#endif
	{
		assert(other._size == other._done && "Can't move a counter that is still being actively used");
	}

	counter& counter::operator=(counter&& other) noexcept
	{
		assert(other._size == other._done && "Can't move a counter that is still being actively used");
		
		_ignore_waiter = other._ignore_waiter;
		_size = (std::size_t)other._size;
		_done = (std::size_t)other._done;
		_fiber = (fiber_base*)other._fiber;
		
#if !defined(NDEBUG)
		_on_wait_end = std::move(other._on_wait_end);
#endif

		return *this;
	}

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

		if (_size)
		{
			auto fiber = this_fiber::instance();
			_fiber.store(fiber, std::memory_order_release);
			fiber->get_fiber_pool()->block({});
		}

#if !defined(NDEBUG)
		_on_wait_end();
		_on_wait_end = [] {};
#endif
	}
}
