#include "pool/fiber_pool.hpp"
#include "core/fiber.hpp"

#include <spdlog/spdlog.h>



namespace np
{
    fiber_pool_base::fiber_pool_base() noexcept :
        _running(false),
        _number_of_spawned_fibers(0),
        _worker_threads(),
        _thread_ids(),
        _dispatcher_fibers(),
        _running_fibers(),
        _fibers(),
        _awaiting_fibers(),
        _tasks(),
        _barrier(0)
    {
        detail::fiber_pool_instance = this;
    }

    fiber* fiber_pool_base::this_fiber() noexcept
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

    void fiber_pool_base::unblock(np::fiber* fiber) noexcept
    {
        _awaiting_fibers.enqueue(fiber);
    }

    void fiber_pool_base::worker_thread(uint8_t idx) noexcept
    {
        plDeclareThreadDyn("Workers/%d", idx);

        // Thread data
        _thread_ids[idx] = std::this_thread::get_id();

        // Wait for all threads
        _barrier.wait();

        // Keep on getting tasks and running them
        while (_running)
        {
            plBegin("Dispatcher loop");

            // Temporal to hold an enqueued fiber
            np::fiber* fiber;

            // Get a free fiber from the pool
            if (!_awaiting_fibers.try_dequeue(fiber))
            {
#if defined(NETPUNK_SPINLOCK_PAUSE)
#if defined(_MSC_VER)
                _mm_pause();
#else
                __builtin_ia32_pause();
#endif
#endif // NETPUNK_SPINLOCK_PAUSE
                // TODO(gpascualg): Check for tasks and empty fibers!
                plEnd("Dispatcher loop");
                continue;
            }

            // Maybe this fiber is yet in the process of yielding, we don't want to execute it if
            //  that is the case
            if (fiber->execution_status({}) != fiber_execution_status::ready)
            {
                _awaiting_fibers.enqueue(fiber);
                plEnd("Dispatcher loop");
                continue;
            }

            spdlog::debug("[{}] FIBER {}/{} EXECUTE", idx, fiber->_id, fiber->status());
            plAttachVirtualThread(fiber->_id);

            // Change execution context
            //plBegin("Fiber execution");
            fiber->execution_status({}, fiber_execution_status::executing);
            _running_fibers[idx] = fiber;
            _dispatcher_fibers[idx]->resume(fiber);
            spdlog::debug("[{}] FIBER {}/{} IS BACK", idx, fiber->_id, fiber->status());

            // Push fiber back
            switch (fiber->status())
            {
                case fiber_status::ended:
                {
                    plBegin("Dispatcher pop task");
                    task_bundle task;
                    if (_tasks.try_dequeue(task))
                    {
                        fiber->reset(std::move(task.function), *task.counter);
                        _awaiting_fibers.enqueue(std::move(fiber));
                    }
                    else
                    {
                        _fibers.enqueue(std::move(fiber));
                    }
                    plEnd("Dispatcher pop task");
                }
                break;

                case fiber_status::yielded:
                    plBegin("Dispatcher push awaiting");
                    _awaiting_fibers.enqueue(std::move(fiber));
                    plEnd("Dispatcher push awaiting");
                    break;

                case fiber_status::blocked:
                    // Do nothing, the mutex will already hold a reference to the fiber
                    break;

                default:
                    assert(false && "Fiber ended with unexpected status");
                    unreachable();
                    abort();
                    break;
            }

            fiber->execution_status({}, fiber_execution_status::ready);
            //plEnd("Fiber execution");
            plEnd("Dispatcher loop");
        }
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
