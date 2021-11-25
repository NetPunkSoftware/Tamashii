#include "synchronization/spinbarrier.hpp"

#include "pool/fiber_pool.hpp"


namespace np
{
	spinbarrier::spinbarrier(std::size_t size) noexcept :
		_size(size)
	{}

	void spinbarrier::reset(std::size_t size) noexcept
	{
		_size = size;
	}

	void spinbarrier::wait() noexcept
	{
		--_size;
		while (_size.load(std::memory_order_relaxed))
		{
#if defined(NETPUNK_SPINBARRIER_PAUSE)
#if defined(_MSC_VER)
			_mm_pause();
#else
			__builtin_ia32_pause();
#endif
#endif // NETPUNK_SPINLOCK_PAUSE
		}
	}
}
