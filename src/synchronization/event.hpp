#pragma once

#include <concurrentqueue.h>


namespace np
{
	class fiber;
	class mutex;

	class event
	{
	public:
		event() noexcept = default;

		void notify() noexcept;
		void wait(np::mutex& mutex) noexcept;

	private:
		np::fiber* _awaiter;
	};
}
