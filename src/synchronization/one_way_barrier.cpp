#include "synchronization/one_way_barrier.hpp"
#include "pool/fiber_pool.hpp"


namespace np
{
	one_way_barrier::one_way_barrier(std::size_t size) noexcept :
		_size(size),
		_fiber(nullptr)
	{}

	void one_way_barrier::reset(std::size_t size) noexcept
	{
		_size = size;
		_fiber = nullptr;
	}

	void one_way_barrier::decrease() noexcept
	{
		if (--_size == 0)
		{
			auto fiber = this_fiber::instance(); 
			np::fiber_base* waiter;
			while (!(waiter = _fiber.load(std::memory_order_relaxed)))
			{
				fiber->get_fiber_pool()->yield();
			}

			waiter->get_fiber_pool()->unblock({}, waiter);
		}
	}

	void one_way_barrier::wait() noexcept
	{
		assert(_fiber.load(std::memory_order_relaxed) == nullptr && "Only one fiber can wait on a one_way_barrier");

		auto fiber = this_fiber::instance();
		_fiber.store(fiber, std::memory_order_release);
		fiber->get_fiber_pool()->block({});
	}
}
