#pragma once

#include "synchronization/condition_variable.hpp"
#include "synchronization/mutex.hpp"

#include <functional>


namespace np
{
	class channel
	{
	public:
		channel() noexcept;

		template <typename F>
		void push(F&& fnc) noexcept;

		void start() noexcept;
		inline void stop() noexcept;

	private:
		np::condition_variable _cv;
		np::mutex _mutex;

		bool _running;
		std::atomic<uint32_t> _pending;
		moodycamel::ConcurrentQueue<std::function<void()>> _jobs;
	};


	template <typename F>
	void channel::push(F&& fnc) noexcept
	{
		_jobs.enqueue(std::forward<F>(fnc));
		_mutex.lock();
		++_pending;
		_cv.notify_one(); // There can only be one fiber waiting, the consumer
		_mutex.unlock();
	}

	inline void channel::stop() noexcept
	{
		_running = false;
	}
}
