#pragma once

#include <concurrentqueue.h>


namespace np
{
	class fiber_base;
	class fiber_pool_base;
	class mutex;

	class event
	{
	public:
		event() noexcept = default;

		void notify() noexcept;
		void wait(np::mutex& mutex) noexcept;

		void notify(fiber_pool_base* fiber_pool) noexcept;
		void wait(fiber_pool_base* fiber_pool, np::mutex& mutex) noexcept;

	private:
		np::fiber_base* _awaiter;
	};
}
