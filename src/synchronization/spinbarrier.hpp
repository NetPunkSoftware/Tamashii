#pragma once

#include "synchronization/mutex.hpp"

#include <atomic>


namespace np
{
	class spinbarrier
	{
	public:
		spinbarrier(std::size_t size) noexcept;

		void reset(std::size_t size) noexcept;
		void wait() noexcept;

	private:
		std::atomic<std::size_t> _size;
	};
}
