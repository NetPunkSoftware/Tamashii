#pragma once

#include "core/fiber.hpp"
#include "utils/badge.hpp"
#include "synchronization/counter.hpp"
#include "synchronization/spinbarrier.hpp"

#include <concurrentqueue.h>

#include <array>
#include <thread>
#include <vector>



#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and other compilers compatible with GCC (-std=c++0x or above)

    [[noreturn]] inline __attribute__((always_inline)) void unreachable() { __builtin_unreachable(); }

    #define NP_NOINLINE __attribute__((noinline))

#elif defined(_MSC_VER) // MSVC

    [[noreturn]] __forceinline void unreachable() { __assume(false); }
    
    #define NP_NOINLINE __declspec(noinline)

#else // ???

    inline void unreachable() {}

    #define NP_NOINLINE

#endif

#if !defined(NDEBUG) || defined(TAMASHII_FIBER_POOL_TRACK_BLOCKED)
    #define TAMASHII_INTERNAL_FIBER_POOL_TRACK_BLOCKED
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
            static const uint32_t maximum_fibers = 300;
            static const uint32_t yield_priority = 2;
            static const uint16_t maximum_threads = 256;

            // Fiber traits
            static const uint32_t inplace_function_size = 64;
            static const uint32_t fiber_stack_size = 524288;
        };

        inline fiber_pool_base* fiber_pool_instance = nullptr;
    }


    class fiber_pool_base
    {
        template <typename traits> friend class fiber_pool;

    public:
        fiber_pool_base(bool create_instance = true) noexcept;

        static NP_NOINLINE uint8_t thread_index() noexcept;
        static fiber_base* this_fiber() noexcept;
        void yield() noexcept;

        using protected_access_t = ::badge<np::mutex, np::one_way_barrier, np::barrier, np::counter, np::condition_variable, np::event>;

        inline void block(protected_access_t) noexcept;
        inline void unblock(protected_access_t, np::fiber_base* fiber) noexcept;
        inline uint16_t number_of_threads() const noexcept;
        inline uint32_t target_number_of_fibers() const noexcept;

        template <typename T>
        static std::array<T, 256>& threadlocal_all() noexcept;

        template <typename T>
        static T& threadlocal() noexcept;

        static uint8_t maximum_worker_id() noexcept;

    protected:
        np::counter* get_dummy_counter() noexcept;

        void block() noexcept;
        void unblock(fiber_base* fiber) noexcept;

        inline constexpr auto badge()
        {
            return ::badge<fiber_pool_base>{};
        }

    // Only fiber_pool may destroy fiber_pool_base
    protected:
        ~fiber_pool_base() noexcept = default;

    protected:
        static std::atomic<uint8_t> _fiber_worker_id;
        static std::array<std::thread::id, 256> _thread_ids;
        static std::array<np::fiber_base*, 256> _running_fibers;
        static std::array<np::fiber_base*, 256> _dispatcher_fibers;

        bool _running;
        bool _with_main_thread;
        uint8_t _main_worker_id;
        uint16_t _number_of_threads;
        uint32_t _number_of_spawned_fibers;
        uint32_t _target_number_of_fibers;
        std::vector<std::thread> _worker_threads;
        moodycamel::ConcurrentQueue<np::fiber_base*> _fibers;
        moodycamel::ConcurrentQueue<np::fiber_base*> _awaiting_fibers;
        np::spinbarrier _barrier;

#ifdef TAMASHII_INTERNAL_FIBER_POOL_TRACK_BLOCKED
        std::atomic<uint32_t> _number_of_blocked_fibers;
#endif

#ifndef NDEBUG
        bool _is_joined;
#endif
    };

    template <typename traits = detail::default_fiber_pool_traits>
    class fiber_pool : public fiber_pool_base
    {
        struct task_bundle
        {
            np::counter* counter;
            stdext::inplace_function<void(), traits::inplace_function_size> function;
        };

    public:
        fiber_pool() noexcept;
        ~fiber_pool() noexcept;

        static fiber_pool<traits>* instance() noexcept;

        void start(uint16_t number_of_threads = 0, bool with_main_thread = true) noexcept;
        void enable_main_thread_calls_here() noexcept;
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

    private:
        moodycamel::ConcurrentQueue<task_bundle> _tasks;
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
    
    template <typename T>
    std::array<T, 256>& fiber_pool_base::threadlocal_all() noexcept
    {
        static std::array<T, 256> thread_local_ts {};
        return thread_local_ts;
    }
    
    template <typename T>
    T& fiber_pool_base::threadlocal() noexcept
    {
        return threadlocal_all<T>()[thread_index()];
    }


    template <typename traits>
    fiber_pool<traits>::fiber_pool() noexcept :
        fiber_pool_base(),
        _tasks()
    {
#if defined(NETPUNK_TAMASHII_LOG)
        spdlog::trace("fiber_pool constructor called");
#endif

        if constexpr (traits::preemtive_fiber_creation)
        {
            np::fiber_base** fibers = new np::fiber_base * [traits::maximum_fibers];
            for (uint32_t i = 0; i < traits::maximum_fibers; ++i)
            {

#if defined(NDEBUG)
                fibers[i] = new np::fiber<traits>(traits::fiber_stack_size, empty_fiber_t{});
#else
                fibers[i] = new np::fiber<traits>(traits::fiber_stack_size, &detail::invalid_fiber_guard);
#endif
            }

            _number_of_spawned_fibers = traits::maximum_fibers;
            _fibers.enqueue_bulk(fibers, traits::maximum_fibers);
            delete[] fibers;
        }
    }

    template <typename traits>
    fiber_pool<traits>::~fiber_pool() noexcept
    {  
#ifndef NDEBUG
        assert(_is_joined && "Pool went out of scope without being joined");
#endif

#if defined(NETPUNK_TAMASHII_LOG)
        spdlog::trace("fiber_pool destructor called");
#endif

        np::fiber_base* fiber_base;
        while (_fibers.try_dequeue(fiber_base))
        {
            auto fiber = reinterpret_cast<np::fiber<traits>*>(fiber_base);
            delete fiber;
        }

        // If main thread did not join, we need to do cleanup now
        if (!_with_main_thread)
        {
            _thread_ids[_main_worker_id] = {};
            auto fiber = reinterpret_cast<np::fiber<traits>*>(_dispatcher_fibers[_main_worker_id]);
            delete fiber;
        }
    }

    template <typename traits>
    fiber_pool<traits>* fiber_pool<traits>::instance() noexcept
    {
        return reinterpret_cast<fiber_pool<traits>*>(detail::fiber_pool_instance);
    }

    template <typename traits>
    void fiber_pool<traits>::start(uint16_t number_of_threads, bool with_main_thread) noexcept
    {
        if (number_of_threads == 0)
        {
            number_of_threads = std::thread::hardware_concurrency();
        }

        _number_of_threads = number_of_threads;
        _target_number_of_fibers = traits::maximum_fibers;

        _running = true;
        _barrier.reset(number_of_threads);

        // Execute in other threads
        for (int idx = int(with_main_thread); idx < number_of_threads; ++idx)
        {
            // Set dispatcher fiber
            uint8_t worker_id = _fiber_worker_id++;
            _dispatcher_fibers[worker_id] = new np::fiber<traits>("Dispatcher/%d", traits::fiber_stack_size, empty_fiber_t{});
            _dispatcher_fibers[worker_id]->set_fiber_pool(badge(), this);
            _running_fibers[worker_id] = _dispatcher_fibers[worker_id];
            _worker_threads.emplace_back(&fiber_pool<traits>::worker_thread, this, worker_id);
        }

        // Even if we don't want main thread execution, assign an id to the thread
        _main_worker_id = _fiber_worker_id++;
        _dispatcher_fibers[_main_worker_id] = new np::fiber<traits>("Dispatcher/%d", traits::fiber_stack_size, empty_fiber_t{});
        _dispatcher_fibers[_main_worker_id]->set_fiber_pool(badge(), this);
        _running_fibers[_main_worker_id] = _dispatcher_fibers[_main_worker_id];

        // Execute in main thread
        _with_main_thread = with_main_thread;
        if (with_main_thread)
        {
            worker_thread(_main_worker_id);
        }
    }

    template <typename traits>
    void fiber_pool<traits>::enable_main_thread_calls_here() noexcept
    {
        for (uint8_t i = 0; i < _fiber_worker_id; ++i)
        {
            assert(_thread_ids[i] != std::this_thread::get_id() && "Only one pool can own the main thread");
        }

        _thread_ids[_main_worker_id] = std::this_thread::get_id();
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
        counter.increase(badge());

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

#if defined(NETPUNK_TAMASHII_LOG)
                spdlog::trace("[-] FIBER {} CREATED", fiber->_id);
#endif
            }
        }

        return true;
    }

    template <typename traits>
    void fiber_pool<traits>::worker_thread(uint8_t idx) noexcept
    {
        plDeclareThreadDyn("Workers/%d", idx);
        plAttachVirtualThread(_dispatcher_fibers[idx]->_id);

#if defined(NETPUNK_TAMASHII_LOG)
        spdlog::trace("[{}] WORKER {}", (uintptr_t)this, idx);
#endif

        // Thread data
        _thread_ids[idx] = std::this_thread::get_id();

        // Wait for all threads
        _barrier.wait();

        // Keep on getting tasks and running them
        while (true)
        {
#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 1
            plScope("Dispatcher loop");
#endif
            // Temporal to hold an enqueued fiber
            np::fiber_base* fiber;

            // Get a free fiber from the pool
            if (!_awaiting_fibers.try_dequeue(fiber))
            {
#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 2
                plScope("Dispatcher cold");
#endif

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
                    // Exit pool if it is not running anymore
                    if (!_running)
                    {
                        break;
                    }

                    continue;
                }

                // Enqueueing and dequeueing a fiber is easier than a task, go that way
                if (!_fibers.try_dequeue(fiber))
                {
#ifdef TAMASHII_INTERNAL_FIBER_POOL_TRACK_BLOCKED
                    if (_number_of_blocked_fibers == _number_of_spawned_fibers)
                    {
                        ++_number_of_spawned_fibers;
                        spdlog::critical("fiber_pool ran out of fibers, increase maximum spawn to {}", _number_of_spawned_fibers);
#if defined(NDEBUG)
                        fiber = new np::fiber<traits>(traits::fiber_stack_size, empty_fiber_t{});
#else
                        fiber = new np::fiber<traits>(traits::fiber_stack_size, &detail::invalid_fiber_guard);
#endif
                    }
                    else
#endif
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

#if defined(NETPUNK_TAMASHII_LOG)
            spdlog::trace("[{}] FIBER {}/{} EXECUTE", idx, fiber->_id, fiber->status());
#endif

            // Change execution context
#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 3
            plBegin("Fiber execution");
#endif

            fiber->execution_status(badge(), fiber_execution_status::executing);
            _running_fibers[idx] = fiber;

            plAttachVirtualThread(fiber->_id);
            _dispatcher_fibers[idx]->resume(this, fiber);
            plAttachVirtualThread(_dispatcher_fibers[idx]->_id);

#if defined(NETPUNK_TAMASHII_LOG)
            spdlog::trace("[{}] FIBER {}/{} IS BACK", idx, fiber->_id, fiber->status());
#endif

            // Push fiber back
            switch (fiber->status())
            {
                case fiber_status::ended:
                {
#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 3
                    plBegin("Dispatcher pop task");
#endif

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

#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 3
                    plEnd("Dispatcher pop task");
#endif
                }
                break;

                case fiber_status::yielded:
#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 3
                    plBegin("Dispatcher push awaiting");
#endif

                    _awaiting_fibers.enqueue(std::move(fiber));

#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 3
                    plEnd("Dispatcher push awaiting");
#endif
                    break;

                case fiber_status::blocked:
#ifdef TAMASHII_INTERNAL_FIBER_POOL_TRACK_BLOCKED
                    ++_number_of_blocked_fibers;
#endif
                    // Do nothing, the mutex will already hold a reference to the fiber
                    break;

                default:
                    assert(false && "Fiber ended with unexpected status");
                    unreachable();
                    abort();
                    break;
            }

            fiber->execution_status(badge(), fiber_execution_status::ready);
            
#if defined(NETPUNK_TAMASHII_PALANTEER_INTERNAL) && NETPUNK_TAMASHII_PALANTEER_INTERNAL >= 3
            plEnd("Fiber execution");
#endif
        }

#if defined(NETPUNK_TAMASHII_LOG)
        spdlog::trace("[{}] FIBERPOOL THREAD {} ENDS", (uintptr_t)this, idx);
#endif

        // Clean up this thread id
        _thread_ids[idx] = {};
        
        // Delete dispatcher fiber
        auto fiber = reinterpret_cast<np::fiber<traits>*>(_dispatcher_fibers[idx]);
        delete fiber;
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

#ifndef NDEBUG
        _is_joined = true;
#endif
    }

    // Convinient wrappers around methods
    namespace this_fiber
    {
        inline fiber_base* instance() noexcept
        {
            assert(fiber_pool_base::this_fiber() && "Must be called inside a fiber");
            return fiber_pool_base::this_fiber();
        }
        
        inline fiber_pool_base* fiber_pool() noexcept
        {
            assert(instance()->get_fiber_pool() && "Must be called inside a fiber");
            return instance()->get_fiber_pool();
        }

        inline void yield() noexcept
        {
            assert(this_fiber::fiber_pool() && "Must be called inside a fiber");
            this_fiber::fiber_pool()->yield();
        }

        template <typename T, typename... Args>
        inline T& threadlocal(Args&&... args) noexcept
        {
            assert(this_fiber::fiber_pool() && "Must be called inside a fiber");
            return this_fiber::fiber_pool()->template threadlocal<T>(std::forward<Args>(args)...);
        }
    }
}
