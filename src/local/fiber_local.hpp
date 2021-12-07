#pragma once

#include "pool/fiber_pool.hpp"


namespace np
{
    template <typename T>
    class fiber_local
    {
    public:
        fiber_local() noexcept;

        T& operator*() noexcept;
        T* operator->() noexcept;

        template <typename F>
        void for_each(F&& functor);

    private:
        T* _data;
    };


    template <typename T>
    fiber_local<T>::fiber_local() :
        _data(new T[np::detail::fiber_pool_instance->target_number_of_fibers()])
    {}

    template <typename T>
    T& fiber_local<T>::operator*() noexcept
    {
        return _data[np::detail::fiber_pool_instance->this_fiber()->id()];
    }

    template <typename T>
    T* fiber_local<T>::operator->() noexcept
    {
        return &_data[np::detail::fiber_pool_instance->this_fiber()->id()];
    }

    template <typename T>
    template <typename F>
    void fiber_local<T>::for_each(F&& functor)
    {
        for (uint32_t i = 0, size = np::detail::fiber_pool_instance->target_number_of_fibers(); i < size; ++i)
        {
            functor(&_data[i]);
        }
    }
}
