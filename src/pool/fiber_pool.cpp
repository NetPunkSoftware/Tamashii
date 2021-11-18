#include "pool/fiber_pool.hpp"
#include "core/fiber.hpp"


namespace np
{
    fiber_pool::fiber_pool() noexcept :
        _running(false),
        _worker_threads(),
        _thread_ids(),
        _fibers(),
        _tasks()
    {
        _instance = this;
    }

    void fiber_pool::start(uint16_t number_of_threads) noexcept
    {
        if (number_of_threads == 0)
        {
            number_of_threads = std::thread::hardware_concurrency();
        }

        _thread_ids.resize(number_of_threads);
        _running_fibers.resize(number_of_threads);
        _dispatcher_fibers.resize(number_of_threads);
        _thread_ids[0] = std::this_thread::get_id();
        _running = true;

        for (int idx = 1; idx < number_of_threads; ++idx)
        {
            // Set dispatcher fiber
            _dispatcher_fibers[idx] = new np::fiber(empty_fiber_t{});

            _worker_threads.emplace_back([this, idx] () {
                // Thread data
                _thread_ids[idx] = std::this_thread::get_id();

                // Get thread locals
                auto dispatcher_fiber = _dispatcher_fibers[idx];
                auto& running_fiber = _running_fibers[idx];

                // Keep on getting tasks and running them
                while (_running)
                {
                    // Get a task
                    std::function<void()> fn;
                    if (!_tasks.try_dequeue(fn))
                    {
                        continue;
                    }

                    // Get a free fiber from the pool
                    np::fiber* fiber;
                    if (!_fibers.try_dequeue(fiber))
                    {
#if defined(NDEBUG)
                        fiber = new np::fiber(empty_fiber_t{});
#else
                        fiber = new np::fiber(&detail::invalid_fiber_guard);
#endif
                    }

                    // Change execution context
                    running_fiber = fiber;
                    fiber->reset(std::move(fn), idx);
                    dispatcher_fiber->resume(fiber);

                    // Push fiber back
                    _fibers.enqueue(std::move(fiber));
                }
            });
        }
    }

    void fiber_pool::end() noexcept
    {
        _running = false;
        for (auto& t : _worker_threads)
        {
            t.join();
        }
    }

    inline fiber* fiber_pool::this_fiber() noexcept
    {
        return _running_fibers[thread_index()];
    }

    inline void fiber_pool::yield() noexcept
    {
        auto fiber = this_fiber();
        //fiber.
    }

    uint8_t fiber_pool::thread_index() noexcept
    {
        for (int i = 0; i < 12; ++i)
        {
            if (_thread_ids[i] == std::this_thread::get_id())
            {
                return i;
            }
        }

        return -1;
    }
}
