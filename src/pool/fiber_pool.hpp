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
    class counter;
    class fiber;
    class fiber_pool_base;
    template <typename traits> class fiber_pool;
    class mutex;
    class one_way_barrier;
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
            static const uint32_t maximum_fibers = 10;
            static const uint32_t yield_priority = 2;
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
        fiber* this_fiber() noexcept;
        void yield() noexcept;

        inline void block(badge<np::mutex, np::one_way_barrier, np::barrier>) noexcept;
        inline void unblock(badge<np::mutex, np::one_way_barrier, np::barrier>, np::fiber* fiber) noexcept;

    protected:
        void worker_thread(uint8_t idx) noexcept;
        np::counter* get_dummy_counter() noexcept;

        void block() noexcept;
        void unblock(np::fiber* fiber) noexcept;

    protected:
        bool _running;
        uint32_t _number_of_spawned_fibers;
        std::vector<std::thread> _worker_threads;
        std::vector<std::thread::id> _thread_ids;
        std::vector<np::fiber*> _dispatcher_fibers;
        std::vector<np::fiber*> _running_fibers;
        moodycamel::ConcurrentQueue<np::fiber*> _fibers;
        moodycamel::ConcurrentQueue<np::fiber*> _awaiting_fibers;
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

    protected:
        bool get_free_fiber(np::fiber*& fiber) noexcept;
    };


    inline void fiber_pool_base::block(badge<np::mutex, np::one_way_barrier, np::barrier>) noexcept
    {
        block();
    }

    inline void fiber_pool_base::unblock(badge<np::mutex, np::one_way_barrier, np::barrier>, np::fiber* fiber) noexcept
    {
        unblock(fiber);
    }

    template <typename traits>
    fiber_pool<traits>::fiber_pool() noexcept :
        fiber_pool_base()
    {
        if constexpr (traits::preemtive_fiber_creation)
        {
            np::fiber** fibers = new np::fiber*[traits::maximum_fibers];
            for (uint32_t i = 0; i < traits::maximum_fibers; ++i)
            {

#if defined(NDEBUG)
                fibers[i] = new np::fiber(traits::fiber_stack_size, empty_fiber_t{});
#else
                fibers[i] = new np::fiber(traits::fiber_stack_size, &detail::invalid_fiber_guard);
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
            _worker_threads.emplace_back(&fiber_pool_base::worker_thread, this, idx);
        }

        // Execute in main thread
        _dispatcher_fibers[0] = new np::fiber("Dispatcher/%d", traits::fiber_stack_size, empty_fiber_t{});
        worker_thread(0);
    }

    template <typename traits>
    template <typename F>
    void fiber_pool<traits>::push(F&& function) noexcept
    {
        np::fiber* fiber;
        if (!get_free_fiber(fiber))
        {
            _tasks.enqueue({
                .counter = get_dummy_counter(),
                .function = std::forward<F>(function)
            });
            return;
        }

        fiber->reset(std::forward<F>(function));
        _awaiting_fibers.enqueue(fiber);
    }

    template <typename traits>
    template <typename F>
    void fiber_pool<traits>::push(F&& function, np::counter& counter) noexcept
    {
        counter.increase<traits>({});

        np::fiber* fiber;
        if (!get_free_fiber(fiber))
        {
            _tasks.enqueue({
                .counter = &counter,
                .function = std::forward<F>(function)
            });
            return;
        }

        fiber->reset(std::forward<F>(function), counter);
        _awaiting_fibers.enqueue(fiber);
    }

    template <typename traits>
    bool fiber_pool<traits>::get_free_fiber(np::fiber*& fiber) noexcept
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
                fiber = new np::fiber(traits::fiber_stack_size, empty_fiber_t{});
#else
                fiber = new np::fiber(traits::fiber_stack_size, &detail::invalid_fiber_guard);
#endif
                spdlog::debug("[{}] FIBER {} CREATED", fiber->_id);
            }
        }

        return true;
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
