#include "pool/fiber_pool.hpp"
#include "core/fiber.hpp"

#include <spdlog/spdlog.h>



namespace np
{
    fiber_pool_base::fiber_pool_base(bool create_instance) noexcept :
        _running(false),
        _number_of_spawned_fibers(0),
        _worker_threads(),
        _thread_ids(),
        _dispatcher_fibers(),
        _running_fibers(),
        _fibers(),
        _awaiting_fibers(),
        _barrier(0)
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
        spdlog::debug("[{}] FIBER {}/{} YIELD", index, fiber->_id, fiber->status());
        fiber->yield(dispatcher);
    }

    void fiber_pool_base::block() noexcept
    {
        auto index = thread_index();
        auto fiber = _running_fibers[index];
        auto dispatcher = _dispatcher_fibers[index];
        spdlog::debug("[{}] FIBER {}/{} BLOCK", index, fiber->_id, fiber->status());
        fiber->yield_blocking(dispatcher);
    }

    void fiber_pool_base::unblock(np::fiber_base* fiber) noexcept
    {
        _awaiting_fibers.enqueue(fiber);
    }

    uint8_t fiber_pool_base::thread_index() const noexcept
    {
        for (int idx = 0, size = _thread_ids.size(); idx < size; ++idx)
        {
            if (_thread_ids[idx] == std::this_thread::get_id())
            {
                return idx;
            }
        }

        unreachable();
        abort();
    }

    np::counter* fiber_pool_base::get_dummy_counter() noexcept
    {
        return &detail::dummy_counter;
    }
}
