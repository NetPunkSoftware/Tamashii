#include "pool/fiber_pool.hpp"
#include "core/fiber.hpp"

#include <spdlog/spdlog.h>


#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and other compilers compatible with GCC (-std=c++0x or above)
[[noreturn]] inline __attribute__((always_inline)) void unreachable() {__builtin_unreachable();}
#elif defined(_MSC_VER) // MSVC
[[noreturn]] __forceinline void unreachable() {__assume(false);}
#else // ???
inline void unreachable() {}
#endif

namespace np
{
    fiber_pool::fiber_pool() noexcept :
        _running(false),
        _number_of_spawned_fibers(0),
        _worker_threads(),
        _thread_ids(),
        _dispatcher_fibers(),
        _running_fibers(),
        _fibers(),
        _yielded_fibers(),
        _tasks()
    {
        detail::fiber_pool_instance = this;
    }
        
    fiber_pool* fiber_pool::instance() noexcept
    {
        return detail::fiber_pool_instance;
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

        plDeclareThread("Workers/0");

        for (int idx = 1; idx < number_of_threads; ++idx)
        {
            // Set dispatcher fiber
            _dispatcher_fibers[idx] = new np::fiber(empty_fiber_t{});

            _worker_threads.emplace_back([this, idx] () {
                plDeclareThreadDyn("Workers/%d", idx);
                
                // Thread data
                _thread_ids[idx] = std::this_thread::get_id();

                // Keep on getting tasks and running them
                while (_running)
                {
                    // Temporal to hold an enqueued fiber
                    np::fiber* fiber;

                    // Get a task
                    std::function<void()> fn;
                    if (!_tasks.try_dequeue(fn))
                    {
                        update_yielded_fibers(idx);
                        continue;
                    }

                    // Get a free fiber from the pool
                    if (!_fibers.try_dequeue(fiber))
                    {
                        if (++_number_of_spawned_fibers > 100)
                        {
                            _number_of_spawned_fibers = 100;
                            update_yielded_fibers(idx);
                            continue;
                        }

#if defined(NDEBUG)
                        fiber = new np::fiber(empty_fiber_t{});
#else
                        fiber = new np::fiber(&detail::invalid_fiber_guard);
#endif
                        spdlog::debug("[{}] FIBER {} CREATED", idx, fiber->_id);
                    }

                    fiber->reset(std::move(fn), idx);
                    execute(fiber, idx);                    
                }
            });
        }
    }

    void fiber_pool::update_yielded_fibers(uint8_t idx) noexcept
    {
        np::fiber* fibers[10];
        uint32_t count = _yielded_fibers.try_dequeue_bulk(fibers, 10);
        for (uint32_t i = 0; i < count; ++i)
        {
            execute(fibers[i], idx);
        }
    }
    
    void fiber_pool::execute(fiber* fiber, uint8_t idx) noexcept
    {
        spdlog::debug("[{}] FIBER {}/{} EXECUTE", idx, fiber->_id, fiber->status());
        plAttachVirtualThread(fiber->_id);
        
        // Change execution context
        _running_fibers[idx] = fiber;
        _dispatcher_fibers[idx]->resume(fiber);
        spdlog::debug("[{}] FIBER {}/{} IS BACK", idx, fiber->_id, fiber->status());

        // Push fiber back
        switch (fiber->status())
        {
            case fiber_status::ended:
                _fibers.enqueue(std::move(fiber));
                break;

            case fiber_status::yielded:
                _yielded_fibers.enqueue(std::move(fiber));
                break;

            default:
                assert(false && "Fiber ended with unexpected status");
                unreachable();
                abort();
                break;
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

    fiber* fiber_pool::this_fiber() noexcept
    {
        return _running_fibers[thread_index()];
    }

    void fiber_pool::yield() noexcept
    {
        auto index = thread_index();
        auto fiber = _running_fibers[index];
        auto dispatcher = _dispatcher_fibers[index];
        spdlog::debug("[{}] FIBER {}/{} YIELD", index, fiber->_id, fiber->status());
        fiber->yield(dispatcher);
    }

    uint8_t fiber_pool::thread_index() const noexcept
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
}
