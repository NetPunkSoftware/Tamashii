#pragma once

#include "core/fiber.hpp"

#include <concurrentqueue.h>

#include <array>
#include <functional>
#include <thread>
#include <vector>


namespace np
{
    class fiber;
    class fiber_pool;
}


namespace np
{
    namespace detail
    {
        inline void invalid_fiber_guard()
        {
            assert(false && "Attempting to use a fiber without task");
        }

        inline fiber_pool* fiber_pool_instance = nullptr;
    }

    class fiber_pool
    {
    public:
        fiber_pool() noexcept;
        static fiber_pool* instance() noexcept;

        void start(uint16_t number_of_threads = 0) noexcept;

        template <typename F>
        void push(F&& function) noexcept
        {
            _tasks.enqueue(std::forward<F>(function));
        }

        void end() noexcept;

        uint8_t thread_index() const noexcept;
        fiber* this_fiber() noexcept;
        void yield() noexcept;

    private:
        void update_yielded_fibers(uint8_t idx) noexcept;
        void execute(fiber* fiber, uint8_t idx) noexcept;

    private:
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


    // Convinient wrappers around methods
    namespace this_fiber
    {
        inline void yield() noexcept
        {
            detail::fiber_pool_instance->yield();
        }
    }
}
