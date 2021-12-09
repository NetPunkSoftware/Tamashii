#pragma once

#include "core/fiber.hpp"
#include "utils/badge.hpp"
#include "synchronization/spinbarrier.hpp"

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
    template <typename traits> class fiber;
    class fiber_base;

    class fiber_pool_base;
    template <typename traits> class fiber_pool;

    class mutex;
    class counter;
    class condition_variable;
    class one_way_barrier;
    class event;
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
            // Fiber pool traits
            static const bool preemtive_fiber_creation = true;
            static const uint32_t maximum_fibers = 10;
            static const uint32_t yield_priority = 2;

            // Fiber traits
            static const uint32_t inplace_function_size = 64;
            static const uint32_t fiber_stack_size = 524288;
        };

        inline fiber_pool_base* fiber_pool_instance = nullptr;
    }


    class fiber_pool_base
    {
        template <typename traits> friend class fiber_pool;

        struct task_bundle
        {
            np::counter* counter;
            std::function<void()> function;
        };

    public:
        fiber_pool_base() noexcept;

        uint8_t thread_index() const noexcept;
        fiber_base* this_fiber() noexcept;
        void yield() noexcept;

        using protected_access_t = ::badge<np::mutex, np::one_way_barrier, np::barrier, np::counter, np::condition_variable, np::event>;

        inline void block(protected_access_t) noexcept;
        inline void unblock(protected_access_t, np::fiber_base* fiber) noexcept;
        inline uint16_t number_of_threads() const noexcept;
        inline uint32_t target_number_of_fibers() const noexcept;

    protected:
        np::counter* get_dummy_counter() noexcept;

        void block() noexcept;
        void unblock(fiber_base* fiber) noexcept;

        inline constexpr auto badge()
        {
            return ::badge<fiber_pool_base>{};
        }

    protected:
        bool _running;
        uint16_t _number_of_threads;
        uint32_t _number_of_spawned_fibers;
        uint32_t _target_number_of_fibers;
        std::vector<std::thread> _worker_threads;
        std::vector<std::thread::id> _thread_ids;
        std::vector<np::fiber_base*> _dispatcher_fibers;
        std::vector<np::fiber_base*> _running_fibers;
        moodycamel::ConcurrentQueue<np::fiber_base*> _fibers;
        moodycamel::ConcurrentQueue<np::fiber_base*> _awaiting_fibers;
        moodycamel::ConcurrentQueue<task_bundle> _tasks;
        np::spinbarrier _barrier;
    };

    template <typename traits = detail::default_fiber_pool_traits>
    class fiber_pool : public fiber_pool_base
    {
    public:
        fiber_pool() noexcept;

        static fiber_pool<traits>* instance() noexcept;

        void start(uint16_t number_of_threads = 0) noexcept;
        void end() noexcept;
        void join() noexcept;

        template <typename F>
        void push(F&& function) noexcept;

        template <typename F>
        void push(F&& function, np::counter& counter) noexcept;

    private:
        void worker_thread(uint8_t idx) noexcept;

    protected:
        bool get_free_fiber(np::fiber_base*& fiber) noexcept;
    };


    inline void fiber_pool_base::block(protected_access_t) noexcept
    {
        block();
    }

    inline void fiber_pool_base::unblock(protected_access_t, np::fiber_base* fiber) noexcept
    {
        unblock(fiber);
    }

    inline uint16_t fiber_pool_base::number_of_threads() const noexcept
    {
        return _number_of_threads;
    }

    inline uint32_t fiber_pool_base::target_number_of_fibers() const noexcept
    {
        return _target_number_of_fibers;
    }

    template <typename traits>
    fiber_pool<traits>::fiber_pool() noexcept :
        fiber_pool_base()
    {
        if constexpr (traits::preemtive_fiber_creation)
        {
            np::fiber_base** fibers = new np::fiber_base*[traits::maximum_fibers];
            for (uint32_t i = 0; i < traits::maximum_fibers; ++i)
            {

#if defined(NDEBUG)
                fibers[i] = new np::fiber<traits>(traits::fiber_stack_size, empty_fiber_t{});
#else
                fibers[i] = new np::fiber<traits>(traits::fiber_stack_size, &detail::invalid_fiber_guard);
#endif
            }

            _fibers.enqueue_bulk(fibers, traits::maximum_fibers);
            delete[] fibers;
        }
    }

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

        _number_of_threads = number_of_threads;
        _target_number_of_fibers = traits::maximum_fibers;
        _thread_ids.resize(number_of_threads);
        _running_fibers.resize(number_of_threads);
        _dispatcher_fibers.resize(number_of_threads);
        _thread_ids[0] = std::this_thread::get_id();
        _running = true;
        _barrier.reset(number_of_threads);

        // Execute in other threads
        for (int idx = 1; idx < number_of_threads; ++idx)
        {
            // Set dispatcher fiber
            _dispatcher_fibers[idx] = new np::fiber("Dispatcher/%d", traits::fiber_stack_size, empty_fiber_t{});
            _worker_threads.emplace_back(&fiber_pool<traits>::worker_thread, this, idx);
        }

        // Execute in main thread
        _dispatcher_fibers[0] = new np::fiber("Dispatcher/%d", traits::fiber_stack_size, empty_fiber_t{});
        worker_thread(0);
    }

    template <typename traits>
    template <typename F>
    void fiber_pool<traits>::push(F&& function) noexcept
    {
        np::fiber_base* fiber;
        if (!get_free_fiber(fiber))
        {
            _tasks.enqueue({
                .counter = get_dummy_counter(),
                .function = std::forward<F>(function)
            });
            return;
        }

        reinterpret_cast<np::fiber<traits>*>(fiber)->reset(std::forward<F>(function));
        _awaiting_fibers.enqueue(fiber);
    }

    template <typename traits>
    template <typename F>
    void fiber_pool<traits>::push(F&& function, np::counter& counter) noexcept
    {
        counter.increase<traits>({});

        np::fiber_base* fiber;
        if (!get_free_fiber(fiber))
        {
            _tasks.enqueue({
                .counter = &counter,
                .function = std::forward<F>(function)
            });
            return;
        }

        reinterpret_cast<np::fiber<traits>*>(fiber)->reset(std::forward<F>(function), counter);
        _awaiting_fibers.enqueue(fiber);
    }

    template <typename traits>
    bool fiber_pool<traits>::get_free_fiber(np::fiber_base*& fiber) noexcept
    {
        if (!_fibers.try_dequeue(fiber))
        {
            if constexpr (traits::preemtive_fiber_creation)
            {
                return false;
            }
            else
            {
                if (++_number_of_spawned_fibers > traits::maximum_fibers)
                {
                    _number_of_spawned_fibers = traits::maximum_fibers;
                    return false;
                }

#if defined(NDEBUG)
                fiber = new np::fiber<traits>(traits::fiber_stack_size, empty_fiber_t{});
#else
                fiber = new np::fiber<traits>(traits::fiber_stack_size, &detail::invalid_fiber_guard);
#endif
                spdlog::debug("[{}] FIBER {} CREATED", fiber->_id);
            }
        }

        return true;
    }

    template <typename traits>
    void fiber_pool<traits>::worker_thread(uint8_t idx) noexcept
    {
        plDeclareThreadDyn("Workers/%d", idx);
        plAttachVirtualThread(_dispatcher_fibers[idx]->_id);

        // Thread data
        _thread_ids[idx] = std::this_thread::get_id();

        // Wait for all threads
        _barrier.wait();

        // Keep on getting tasks and running them
        while (_running)
        {
            plScope("Dispatcher loop");

            // Temporal to hold an enqueued fiber
            np::fiber_base* fiber;

            // Get a free fiber from the pool
            if (!_awaiting_fibers.try_dequeue(fiber))
            {
                plScope("Dispatcher cold");

#if defined(NETPUNK_SPINLOCK_PAUSE)
#if defined(_MSC_VER)
                _mm_pause();
#else
                __builtin_ia32_pause();
#endif
#endif // NETPUNK_SPINLOCK_PAUSE

                // Try to get a new task without assigned fiber
                // But first try an early out
                if (_tasks.size_approx() == 0)
                {
                    continue;
                }

                // Enqueueing and dequeueing a fiber is easier than a task, go that way
                if (!_fibers.try_dequeue(fiber))
                {
                    continue;
                }

                task_bundle task;
                if (!_tasks.try_dequeue(task))
                {
                    _fibers.enqueue(fiber);
                    continue;
                }

                // We had a fiber and a task!
                reinterpret_cast<np::fiber<traits>*>(fiber)->reset(std::move(task.function), *task.counter);
                _awaiting_fibers.enqueue(std::move(fiber));
                continue;
            }

            // Maybe this fiber is yet in the process of yielding, we don't want to execute it if
            //  that is the case
            if (fiber->execution_status(badge()) != fiber_execution_status::ready)
            {
                _awaiting_fibers.enqueue(fiber);
                continue;
            }

            spdlog::debug("[{}] FIBER {}/{} EXECUTE", idx, fiber->_id, fiber->status());

            // Change execution context
            plBegin("Fiber execution");
            fiber->execution_status(badge(), fiber_execution_status::executing);
            _running_fibers[idx] = fiber;

            plAttachVirtualThread(fiber->_id);
            _dispatcher_fibers[idx]->resume(fiber);
            plAttachVirtualThread(_dispatcher_fibers[idx]->_id);

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
                    reinterpret_cast<np::fiber<traits>*>(fiber)->reset(std::move(task.function), *task.counter);
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

            fiber->execution_status(badge(), fiber_execution_status::ready);
            plEnd("Fiber execution");
        }
    }

    template <typename traits>
    void fiber_pool<traits>::end() noexcept
    {
        _running = false;
    }

    template <typename traits>
    void fiber_pool<traits>::join() noexcept
    {
        for (auto& t : _worker_threads)
        {
            t.join();
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
