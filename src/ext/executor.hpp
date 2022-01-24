#pragma once

#include "synchronization/condition_variable.hpp"
#include "synchronization/mutex.hpp"

#include <functional>
#include <inplace_function.h>


namespace np
{
	template <uint32_t inplace_function_size = 32>
	class executor
	{
		using job_t = stdext::inplace_function<void(), inplace_function_size>;

	public:
		executor() noexcept;

		template <typename F>
		void push(F&& fnc) noexcept;

		void start() noexcept;
		inline void stop() noexcept;
		inline void join() noexcept;

	private:
		np::condition_variable _cv;
		np::mutex _mutex;

		bool _running;
		std::atomic<uint32_t> _running_count;
		std::atomic<uint32_t> _pending;
		moodycamel::ConcurrentQueue<job_t> _jobs;
	};

	template <uint32_t inplace_function_size>
	executor<inplace_function_size>::executor() noexcept :
		_cv(),
		_mutex(),
		_running(false),
		_pending(0),
		_jobs()
	{}

	template <uint32_t inplace_function_size>
	template <typename F>
	void executor<inplace_function_size>::push(F&& fnc) noexcept
	{
		_jobs.enqueue(std::forward<F>(fnc));
		_mutex.lock();
		++_pending;
		_cv.notify_all();
		_mutex.unlock();
	}

	template <uint32_t inplace_function_size>
	void executor<inplace_function_size>::start() noexcept
	{
		++_running_count;
		_running = true;

		while (_running || _pending)
		{
			// Wait until there is some work
			_mutex.lock();
			// Make sure we are not late to the party
			while (_running && _pending == 0)
			{
				_cv.wait(_mutex);
			}
			_mutex.unlock();

			// Pop jobs
			job_t fnc;
			while (_jobs.try_dequeue(fnc))
			{
				fnc();
				--_pending;
			}
		}

		_mutex.lock();
		--_running_count;
		_cv.notify_all();
		_mutex.unlock();
	}

	template <uint32_t inplace_function_size>
	inline void executor<inplace_function_size>::stop() noexcept
	{
		_running = false;

		_mutex.lock();
		_cv.notify_all();
		_mutex.unlock();
	}

	template <uint32_t inplace_function_size>
	inline void executor<inplace_function_size>::join() noexcept
	{
		// Wait until there is some work
		_mutex.lock();
		// Make sure we are not late to the party
		while (_running_count != 0 || _pending != 0)
		{
			_cv.wait(_mutex);
		}
		_mutex.unlock();
	}
}
