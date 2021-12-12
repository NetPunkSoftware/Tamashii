#pragma once

#include <concurrentqueue.h>


namespace np
{
	class fiber_base;
	class fiber_pool_base;
	class mutex;

	class condition_variable
	{
	public:
		condition_variable() noexcept = default;

		void notify_one() noexcept;
		void notify_all() noexcept;
		void wait(np::mutex& mutex) noexcept;

		// Explicit methods
		void notify_one(fiber_pool_base* fiber_pool) noexcept;
		void notify_all(fiber_pool_base* fiber_pool) noexcept;
		void wait(fiber_pool_base* fiber_pool, np::mutex& mutex) noexcept;

	private:
		moodycamel::ConcurrentQueue<np::fiber_base*> _waiting_fibers;
	};
}
