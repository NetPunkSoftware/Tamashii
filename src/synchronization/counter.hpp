#pragma once

#include "synchronization/mutex.hpp"
#include "utils/badge.hpp"

#include <atomic>


namespace np
{
	class fiber;
	template <typename traits> class fiber_pool;

	class counter
	{
	public:
		counter() noexcept;

		template <typename T>
		void increase(badge<fiber_pool<T>>) noexcept
		{
			++_total;
		}

		void reset() noexcept;
		void done(badge<fiber>) noexcept;
		void wait() noexcept;

	private:
		std::atomic<bool> _waiting;
		std::atomic<std::size_t> _total;
		std::atomic<std::size_t> _done;
		np::mutex _mutex;
	};
}
