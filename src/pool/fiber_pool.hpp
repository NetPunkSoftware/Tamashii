#pragma once

#include "container/spmc_queue.hpp"
#include "core/fiber.hpp"

#include <array>
#include <thread>
#include <vector>


namespace np
{
    namespace detail
    {
        inline void invalid_fiber_guard()
        {
            assert(false && "Attempting to use a fiber without task");
        }
    }

    struct thread_data
    {
        thread_data() :
            fibers(),
            tasks()
        {}

        spmc_queue<np::fiber> fibers;
        spmc_queue<std::function<void()>> tasks;
    };

    class fiber_pool
    {
    public:
        fiber_pool() :
            _worker_threads(),
            _thread_data()
        {}

        void start()
        {
            _thread_ids[0] = std::this_thread::get_id();

            for (int i = 1; i < 3; ++i)
            {
                auto& tid = _thread_ids[i];
                auto& data = _thread_data[i];

                _worker_threads.emplace_back([&tid, &data] () {
                    // Set ID
                    tid = std::this_thread::get_id();

                    // Create main fiber
                    np::fiber main;

                    // Create auxiliar fibers
                    for (int i = 0; i < 512; ++i)
                    {
                        data.fibers.push(new np::fiber(&detail::invalid_fiber_guard));
                    }

                    while (true)
                    {
                        // Get a task
                        auto task = data.tasks.pop();
                        if (!task)
                        {
                            continue;
                        }

                        // Get a free fiber from the pool
                        auto fiber = data.fibers.pop();
                        if (!fiber)
                        {
                            // There are no free fibers, create a new one
                            fiber = new np::fiber()
                        }
                    }
                });
            }
        }

    private:
        uint8_t thread_index()
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

    private:
        std::vector<std::thread> _worker_threads;
        std::array<thread_data, 12> _thread_data;
        std::array<std::thread::id, 12> _thread_ids;
    };
}
