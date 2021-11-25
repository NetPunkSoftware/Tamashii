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
			np::fiber* waiter;
			while (!(waiter = _fiber.load(std::memory_order_relaxed)))
			{
				detail::fiber_pool_instance->yield();
			}

			detail::fiber_pool_instance->unblock({}, waiter);
		}
	}

	void one_way_barrier::wait() noexcept
	{
		assert(_fiber.load(std::memory_order_relaxed) == nullptr && "Only one fiber can wait on a one_way_barrier");

		_fiber.store(detail::fiber_pool_instance->this_fiber(), std::memory_order_release);
		detail::fiber_pool_instance->block({});
	}
}
