#pragma once

#include "synchronization/event.hpp"
#include "synchronization/mutex.hpp"


namespace np
{
	template <typename T>
	class channel
	{
	public:
		channel() noexcept;

		void push(T&& value) noexcept;
		void push_blocking(T&& value) noexcept;

		bool pop(T&& value) noexcept;
		void pop_blocking(T&& value) noexcept;

	private:
		np::event _ev;
		np::mutex _mutex;

		uint32_t _pending;
		moodycamel::ConcurrentQueue<T> _queue;
	};


	template <typename T>
	channel<T>::channel() noexcept :
		_ev(),
		_mutex(),
		_pending(0),
		_queue()
	{}

	template <typename T>
	void channel<T>::push(T&& value) noexcept
	{
		_queue.enqueue(std::forward<T>(value));
	}

	template <typename T>
	void channel<T>::push_blocking(T&& value) noexcept
	{
		_queue.enqueue(std::forward<T>(value));
		_mutex.lock();
		++_pending;
		_ev.notify();
		_mutex.unlock();
	}

	template <typename T>
	bool channel<T>::pop(T&& value) noexcept
	{
		return _queue.try_dequeue(std::forward<T>(value));
	}

	template <typename T>
	void channel<T>::pop_blocking(T&& value) noexcept
	{
		// Wait until there is some work
		_mutex.lock();
		// This could be a 'while' instead of an 'if', but we are the only consumer
		//	so there is no worries that someone could beat us and decrease _pending
		if (_pending == 0)
		{
			_ev.wait(_mutex);
		}
		--_pending;
		_mutex.unlock();

		// Pop jobs
		return _queue.try_dequeue(std::forward<T>(value));
	}
}
