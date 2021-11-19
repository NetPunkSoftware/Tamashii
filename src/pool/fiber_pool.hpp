#pragma once

#include "core/fiber.hpp"

#include <concurrentqueue.h>

#include <array>
#include <functional>
#include <thread>
#include <vector>



#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and other compilers compatible with GCC (-std=c++0x or above)
[[noreturn]] inline __attribute__((always_inline)) void unreachable() { __builtin_unreachable(); }
#elif defined(_MSC_VER) // MSVC
[[noreturn]] __forceinline void unreachable() { __assume(false); }
#else // ???
inline void unreachable() {}
#endif


namespace np
{
    class fiber;
    class fiber_pool_base;
    template <typename traits> class fiber_pool;
}


namespace np
{
    namespace detail
    {
        inline void invalid_fiber_guard()
        {
            assert(false && "Attempting to use a fiber without task");
        }

        struct default_fiber_pool_traits
        {
            static const bool preemtive_fiber_creation = true;
            static const uint32_t maximum_fibers = 100;
            static const uint32_t yield_priority = 2;
        };

        inline fiber_pool_base* fiber_pool_instance = nullptr;
    }

    class fiber_pool_base
    {
    public:
        fiber_pool_base() noexcept;

        uint8_t thread_index() const noexcept;
        fiber* this_fiber() noexcept;
        void yield() noexcept;

        template <typename F>
        void push(F&& function) noexcept
        {
            _tasks.enqueue(std::forward<F>(function));
        }

    protected:
        bool _running;
        uint32_t _number_of_spawned_fibers;
        std::vector<std::thread> _worker_threads;
        std::vector<std::thread::id> _thread_ids;
        std::vector<np::fiber*> _dispatcher_fibers;
        std::vector<np::fiber*> _running_fibers;
        moodycamel::ConcurrentQueue<np::fiber*> _fibers;
        moodycamel::ConcurrentQueue<np::fiber*> _yielded_fibers;
        moodycamel::ConcurrentQueue<std::function<void()>> _tasks;
    };

    template <typename traits = detail::default_fiber_pool_traits>
    class fiber_pool : public fiber_pool_base
    {
    public:
        using fiber_pool_base::fiber_pool_base;

        static fiber_pool<traits>* instance() noexcept;

        void start(uint16_t number_of_threads = 0) noexcept;
        void end() noexcept;

    private:
        void update_yielded_fibers(uint8_t idx) noexcept;
        void execute(fiber* fiber, uint8_t idx) noexcept;
    };


    template <typename traits>
    fiber_pool<traits>* fiber_pool<traits>::instance() noexcept
    {
        return reinterpret_cast<fiber_pool<traits>*>(detail::fiber_pool_instance);
    }

    template <typename traits>
    void fiber_pool<traits>::start(uint16_t number_of_threads) noexcept
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

        if constexpr (traits::preemtive_fiber_creation)
        {
            np::fiber** fibers = new np::fiber*[traits::maximum_fibers];
            for (uint32_t i = 0; i < traits::maximum_fibers; ++i)
            {

#if defined(NDEBUG)
                fibers[i] = new np::fiber(empty_fiber_t{});
#else
                fibers[i] = new np::fiber(&detail::invalid_fiber_guard);
#endif
            }

            _fibers.enqueue_bulk(fibers, traits::maximum_fibers);
            delete[] fibers;
        }

        for (int idx = 1; idx < number_of_threads; ++idx)
        {
            // Set dispatcher fiber
            _dispatcher_fibers[idx] = new np::fiber(empty_fiber_t{});

            _worker_threads.emplace_back([this, idx]() {
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
                        if constexpr (traits::preemtive_fiber_creation)
                        {
                            update_yielded_fibers(idx);
                            continue;
                        }
                        else
                        {
                            if (++_number_of_spawned_fibers > traits::maximum_fibers)
                            {
                                _number_of_spawned_fibers = traits::maximum_fibers;
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
                    }

                    fiber->reset(std::move(fn), idx);
                    execute(fiber, idx);
                }
                });
        }
    }

    template <typename traits>
    void fiber_pool<traits>::end() noexcept
    {
        _running = false;
        for (auto& t : _worker_threads)
        {
            t.join();
        }
    }

    template <typename traits>
    void fiber_pool<traits>::update_yielded_fibers(uint8_t idx) noexcept
    {
        np::fiber* fibers[10];
        uint32_t count = _yielded_fibers.try_dequeue_bulk(fibers, 10);
        for (uint32_t i = 0; i < count; ++i)
        {
            execute(fibers[i], idx);
        }
    }

    template <typename traits>
    void fiber_pool<traits>::execute(fiber* fiber, uint8_t idx) noexcept
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

    // Convinient wrappers around methods
    namespace this_fiber
    {
        inline void yield() noexcept
        {
            detail::fiber_pool_instance->yield();
        }
    }
}
