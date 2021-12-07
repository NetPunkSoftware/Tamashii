#include "ext/executor.hpp"


namespace np
{
	executor::executor() noexcept :
		_ev(),
		_mutex(),
		_running(false),
		_pending(0),
		_jobs()
	{}

	void executor::start() noexcept
	{
		assert(!_running && "Only one fiber can be a consumer");
		_running = true;

		while (_running)
		{
			// Wait until there is some work
			_mutex.lock();
			// This could be a 'while' instead of an 'if', but we are the only consumer
			//	so there is no worries that someone could beat us and decrease _pending
			if (_pending == 0)
			{
				_ev.wait(_mutex);
			}
			_mutex.unlock();

			// Pop jobs
			std::function<void()> fnc;
			while (_jobs.try_dequeue(fnc))
			{
				fnc();
				--_pending;
			}
		}
	}
}
