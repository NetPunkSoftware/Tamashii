#pragma once

#include <concurrentqueue.h>

#include <array>
#include <functional>
#include <thread>
#include <vector>


namespace np
{
    class fiber;
}


namespace np
{
    namespace detail
    {
        inline void invalid_fiber_guard()
        {
            assert(false && "Attempting to use a fiber without task");
        }
    }

    class fiber_pool
    {
    public:
        fiber_pool() noexcept;

        void start(uint16_t number_of_threads = 0) noexcept;

        template <typename F>
        void push(F&& function) noexcept
        {
            _tasks.enqueue(std::forward<F>(function));
        }

        void end() noexcept;

        inline fiber* this_fiber() noexcept;
        inline void yield() noexcept;

    private:
        uint8_t thread_index() noexcept;

    private:
        static inline fiber_pool* _instance = nullptr;

        bool _running;
        std::vector<std::thread> _worker_threads;
        std::vector<std::thread::id> _thread_ids;
        std::vector<np::fiber*> _dispatcher_fibers;
        std::vector<np::fiber*> _running_fibers;
        moodycamel::ConcurrentQueue<np::fiber*> _fibers;
        moodycamel::ConcurrentQueue<std::function<void()>> _tasks;
    };
}
