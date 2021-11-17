#include "core/fiber.hpp"
#include "pool/fiber_pool.hpp"

#include <iostream>


void f1()
{
    std::cout << "F1" << std::endl;
}
void f2(np::fiber& org)
{
    std::cout << "F2 - EARLY" << std::endl;
    np::fiber back(&f1);
    org.resume(&back);
    std::cout << "F2 - LATE" << std::endl;
}
void f3()
{
    std::cout << "F3" << std::endl;
}


int main()
{
    np::fiber main;
    np::fiber first(&f1);
    np::fiber second([&first]() { f2(first); });
    np::fiber third(&f3);
    main.resume(&first).resume(&second).resume(&third);

    np::fiber_pool pool;

    return 0;
}
