#include "core/fiber.hpp"
#include "pool/fiber_pool.hpp"

#include <iostream>

std::atomic<std::size_t> global_counter = 0;

void f1()
{
    std::cout << "F1" << std::endl;
    global_counter++;
}
void f2()
{
    std::cout << "F2 - EARLY" << std::endl;
    np::fiber back(&f1);
    //org.suspend();
    std::cout << "F2 - LATE" << std::endl;
}
void f3()
{
    std::cout << "F3" << std::endl;
    global_counter++;
}


int main()
{
    np::fiber main;
    np::fiber first(&f1);
    np::fiber second(&f2);
    np::fiber third(&f3);
    main.resume(&first);
    main.resume(&second);
    main.resume(&third);

    //np::fiber_pool pool;
    //pool.start();

    //global_counter = 0;
    //int max_iters = 10000;
    //for (int i = 0; i < max_iters; ++i)
    //{
    //    pool.push(&f3);
    //    pool.push(&f1);
    //}

    //std::this_thread::sleep_for(std::chrono::seconds(10));
    //pool.end();

    //assert(global_counter == max_iters * 2);

    return 0;
}
