#define PL_IMPLEMENTATION 1
#include <palanteer.h>

#include "core/fiber.hpp"
#include "pool/fiber_pool.hpp"
#include "synchronization/channel.hpp"
#include "synchronization/mutex.hpp"
#include "synchronization/one_way_barrier.hpp"
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


inline int fib(int n) noexcept
{
    if (n <= 1)
        return n;
    return fib(n - 1) + fib(n - 2);
}

np::channel channel;
uint32_t channel_count;

void f1_yield()
{
    per_thread_count[np::detail::fiber_pool_instance->thread_index()]++;

    plBegin("F1");
    plBegin("F1 EARLY");
    spdlog::trace("F1 - EARLY");
    plEnd("F1 EARLY");
    np::this_fiber::yield();

    channel.push([] { ++channel_count; });

    global_counter++;
    per_thread_count[np::detail::fiber_pool_instance->thread_index()]++;

    plBegin("F1 LATE");
    spdlog::trace("F1 - LATE = {}", fib(25));
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

    channel.push([] { ++channel_count; });

    global_counter++;
    per_thread_count[np::detail::fiber_pool_instance->thread_index()]++;
    plBegin("F2 LATE");
    spdlog::trace("F2 - LATE = {}", fib(25));
    plEnd("F2 LATE");
    plEnd("F2");
}

void f1_spinlock(np::spinlock* mutex)
{
    mutex->lock(std::chrono::seconds(100));
    spdlog::critical("Entered f1 spinlock");
    std::this_thread::sleep_for(std::chrono::seconds(10)); // HACK: DONT
    mutex->unlock();
}

void f2_spinlock(np::spinlock* mutex)
{
    mutex->lock(std::chrono::seconds(100));
    spdlog::critical("Entered f2 spinlock");
    std::this_thread::sleep_for(std::chrono::seconds(10)); // HACK: DONT
    mutex->unlock();
}

std::atomic<uint8_t> in_critical_section = 0;

void f1_mutex(np::mutex* mutex)
{
    mutex->lock();
    spdlog::critical("Entered f1 lock");
    assert(++in_critical_section == 1);
    std::this_thread::sleep_for(std::chrono::seconds(1)); // HACK: DONT
    --in_critical_section;
    mutex->unlock();
    spdlog::critical("Unlocked f1 lock");
}

void f2_mutex(np::mutex* mutex)
{
    mutex->lock();
    spdlog::critical("Entered f2 lock");
    assert(++in_critical_section == 1);
    std::this_thread::sleep_for(std::chrono::seconds(1)); // HACK: DONT
    --in_critical_section;
    mutex->unlock();
    spdlog::critical("Unlocked f2 lock");
}

void f3_mutex(np::mutex* mutex)
{
    mutex->lock();
    spdlog::critical("Entered f3 lock");
    assert(++in_critical_section == 1);
    std::this_thread::sleep_for(std::chrono::seconds(1)); // HACK: DONT
    --in_critical_section;
    mutex->unlock();
    spdlog::critical("Unlocked f3 lock");
}

std::atomic<uint8_t> count_barrier = 0;

void f1_one_way_barrier(np::one_way_barrier* one_way_barrier)
{
    std::this_thread::sleep_for(std::chrono::seconds(1)); // HACK: DONT
    ++count_barrier;
    spdlog::critical("Entered f1 oneway");
    one_way_barrier->decrease();
}

void f2_one_way_barrier(np::one_way_barrier* one_way_barrier)
{
    std::this_thread::sleep_for(std::chrono::seconds(1)); // HACK: DONT
    ++count_barrier;
    spdlog::critical("Entered f2 oneway");
    one_way_barrier->decrease();
}

void f3_one_way_barrier(np::one_way_barrier* one_way_barrier)
{
    std::this_thread::sleep_for(std::chrono::seconds(1)); // HACK: DONT
    ++count_barrier;
    spdlog::critical("Entered f3 oneway");
    one_way_barrier->decrease();
}

void main_fiber(np::fiber_pool<>& pool)
{
    plScope("Main");
    
    np::one_way_barrier one_way_barrier(3);
    pool.push([&one_way_barrier]() { f1_one_way_barrier(&one_way_barrier); });
    pool.push([&one_way_barrier]() { f2_one_way_barrier(&one_way_barrier); });
    pool.push([&one_way_barrier]() { f3_one_way_barrier(&one_way_barrier); });

    spdlog::critical("WAITING");
    one_way_barrier.wait();
    assert(count_barrier == 3);
    spdlog::critical("DONE");

    np::counter counter;
    global_counter = 0;
    int max_iters = 200;
    for (int i = 0; i < max_iters; ++i)
    {
        pool.push(&f1_yield, counter);
        pool.push(&f2_yield, counter);
    }

    spdlog::critical("WAIT {}", max_iters);
    counter.wait();
    assert(global_counter == max_iters * 2);
    spdlog::critical("DONE");

    assert(channel_count == 0);

    counter.reset();
    pool.push([] { channel.start(); }, counter);
    pool.push([] { channel.push([] { channel.stop(); }); });
    counter.wait();

    assert(channel_count == max_iters * 2);

    pool.end();
}


int main()
{
    spdlog::set_level(spdlog::level::level_enum::critical);
    plInitAndStart("Fibers");

    constexpr const uint8_t thread_num = 6;
    np::fiber_pool<> pool;
    pool.push([&pool] { main_fiber(pool); });
    pool.start(thread_num);
    pool.join();

    plStopAndUninit();

    return 0;
}
