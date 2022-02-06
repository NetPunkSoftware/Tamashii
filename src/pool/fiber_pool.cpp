#include "pool/fiber_pool.hpp"
#include "core/fiber.hpp"

#include <spdlog/spdlog.h>



namespace np
{
    std::atomic<uint8_t> fiber_pool_base::_fiber_worker_id = 0;
    std::array<std::thread::id, 256> fiber_pool_base::_thread_ids {};
    std::array<np::fiber_base*, 256> fiber_pool_base::_running_fibers {};
    std::array<np::fiber_base*, 256> fiber_pool_base::_dispatcher_fibers {};

    fiber_pool_base::fiber_pool_base(bool create_instance) noexcept :
        _running(false),
        _number_of_threads(0),
        _number_of_spawned_fibers(0),
        _target_number_of_fibers(0),
        _worker_threads(),
        _fibers(),
        _awaiting_fibers(),
        _barrier(0)
#ifndef NDEBUG
        , _is_joined(false)
#endif
    {
        if (create_instance)
        {
            detail::fiber_pool_instance = this;
        }
    }
    
    fiber_base* fiber_pool_base::this_fiber() noexcept
    {
        return _running_fibers[thread_index()];
    }

    void fiber_pool_base::yield() noexcept
    {
        auto index = thread_index();
        auto fiber = _running_fibers[index];
        auto dispatcher = _dispatcher_fibers[index];

#if defined(NETPUNK_TAMASHII_LOG)
        spdlog::trace("[{}] FIBER {}/{} YIELD", index, fiber->_id, fiber->status());
#endif

        fiber->yield(dispatcher);
    }

    void fiber_pool_base::block() noexcept
    {
        auto index = thread_index();
        auto fiber = _running_fibers[index];
        auto dispatcher = _dispatcher_fibers[index];

#if defined(NETPUNK_TAMASHII_LOG)
        spdlog::trace("[{}] FIBER {}/{} BLOCK", index, fiber->_id, fiber->status());
#endif

        fiber->yield_blocking(dispatcher);
    }

    void fiber_pool_base::unblock(np::fiber_base* fiber) noexcept
    {
        _awaiting_fibers.enqueue(fiber);
    }

    uint8_t NP_NOINLINE fiber_pool_base::thread_index() noexcept
    {
        auto tid = std::this_thread::get_id();
        for (int idx = 0, size = _fiber_worker_id; idx < size; ++idx)
        {
            if (_thread_ids[idx] == tid)
            {
                return idx;
            }
        }

        abort();
        unreachable();
    }

    np::counter* fiber_pool_base::get_dummy_counter() noexcept
    {
        return &detail::dummy_counter;
    }

    uint8_t fiber_pool_base::maximum_worker_id() noexcept
    {
        return _fiber_worker_id;
    }
}
