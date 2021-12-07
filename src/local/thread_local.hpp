#pragma once

#include "pool/fiber_pool.hpp"


namespace np
{
    template <typename T>
    class local
    {
    public:
        local() noexcept;

        T& operator*() noexcept;
        T* operator->() noexcept;

        template <typename F>
        void for_each(F&& functor);
        
    private:
        T* _data;
    };


    template <typename T>
    local<T>::local() :
        _data(new T[np::detail::fiber_pool_instance->number_of_threads()])
    {}

    template <typename T>
    T& local<T>::operator*() noexcept
    {
        return _data[np::detail::fiber_pool_instance->thread_index()];
    }

    template <typename T>
    T* local<T>::operator->() noexcept
    {
        return &_data[np::detail::fiber_pool_instance->thread_index()];
    }

    template <typename T>
    template <typename F>
    void local<T>::for_each(F&& functor)
    {
        for (uint32_t i = 0, size = np::detail::fiber_pool_instance->number_of_threads(); i < size; ++i)
        {
            functor(&_data[i]);
        }
    }
}
