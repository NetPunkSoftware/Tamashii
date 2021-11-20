#define PL_IMPLEMENTATION 1
#include <palanteer.h>

#include "core/fiber.hpp"
#include "pool/fiber_pool.hpp"
#include "synchronization/mutex.hpp"
#include "synchronization/spinlock.hpp"

#include <spdlog/spdlog.h>

#include <iostream>


std::atomic<std::size_t> global_counter = 0;

void f1()
{
    spdlog::trace("F1");
    global_counter++;
}
void f2()
{
    spdlog::trace("F2 - EARLY");
    np::fiber back(&f1);
    //org.suspend();
    spdlog::trace("F2 - LATE");
}
void f3()
{
    spdlog::trace("F3");
    global_counter++;
}

void f_resumable(np::fiber* fiber)
{
    spdlog::trace("EARLY");
    fiber->yield();
    spdlog::trace("LATE");
}

std::atomic<uint32_t> per_thread_count[12] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
};

void f1_yield()
{
    per_thread_count[np::detail::fiber_pool_instance->thread_index()]++;

    plBegin("F1");
    plBegin("F1 EARLY");
    spdlog::trace("F1 - EARLY");
    plEnd("F1 EARLY");
    np::this_fiber::yield();

    per_thread_count[np::detail::fiber_pool_instance->thread_index()]++;
    plBegin("F1 LATE");
    spdlog::trace("F1 - LATE");
    plEnd("F1 LATE");
    plEnd("F1");
}
void f2_yield()
{
    per_thread_count[np::detail::fiber_pool_instance->thread_index()]++;
    plBegin("F2");
    plBegin("F2 EARLY");
    spdlog::trace("F2 - EARLY");
    plEnd("F2 EARLY");
    np::this_fiber::yield();

    per_thread_count[np::detail::fiber_pool_instance->thread_index()]++;
    plBegin("F2 LATE");
    spdlog::trace("F2 - LATE");
    plEnd("F2 LATE");
    plEnd("F2");
}

void f1_mutex(np::spinlock* mutex)
{
    mutex->lock(std::chrono::seconds(100));
    spdlog::critical("Entered f1 lock");
    std::this_thread::sleep_for(std::chrono::seconds(10)); // HACK: DONT
    mutex->unlock();
}

void f2_mutex(np::spinlock* mutex)
{
    mutex->lock(std::chrono::seconds(100));
    spdlog::critical("Entered f2 lock");
    std::this_thread::sleep_for(std::chrono::seconds(10)); // HACK: DONT
    mutex->unlock();
}

int main()
{
    spdlog::set_level(spdlog::level::level_enum::critical);

    // np::fiber main;
    // np::fiber first(&f1);
    // np::fiber second(&f2);
    // np::fiber third(&f3);
    // main.resume(&first);
    // main.resume(&second);
    // main.resume(&third);

    // auto resumable = new np::fiber(np::empty_fiber_t{});
    // resumable->reset([resumable] () { f_resumable(resumable); }, 0);
    // main.resume(resumable);
    // spdlog::trace("HAS YIELDED");
    // main.resume(resumable);
    // spdlog::trace("DONE");

    plInitAndStart("Fibers");

    constexpr const uint8_t thread_num = 6;
    np::fiber_pool<> pool;
    pool.start(thread_num);

    //pool.push(&f1_yield);
    //std::this_thread::sleep_for(std::chrono::seconds(1));

    // pool.push(&f2_yield);
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    // pool.push(&f1_yield);
    // pool.push(&f2_yield);
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    np::spinlock mutex;
    pool.push([&mutex]() { f1_mutex(&mutex); });
    pool.push([&mutex]() { f2_mutex(&mutex); });
    std::this_thread::sleep_for(std::chrono::seconds(30));

    global_counter = 0;
    int max_iters = 10000;
    for (int i = 0; i < max_iters; ++i)
    {
       pool.push(&f1_yield);
       pool.push(&f2_yield);
    }

    spdlog::critical("All submited");
    std::this_thread::sleep_for(std::chrono::seconds(30));
    pool.end();

    for (uint8_t i = 0; i < thread_num; ++i)
    {
        spdlog::critical("Thread {}\t{}", i, per_thread_count[i]);
    }

    plStopAndUninit();

    return 0;
}
