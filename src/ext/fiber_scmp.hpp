#pragma once

#include "local/fiber_local.hpp"

#include <vector>


namespace np
{
    template <typename T>
    class scmp
    {
    public:
        void push(T&& value);
        
        template <typename F>
        void consume(F&& functor);

    private:
        fiber_local<std::vector<T>> _queues;
    };

    
    template <typename T>
    void scmp<T>::push(T&& value)
    {
        _queues->emplace_back(std::forward<T>(value));
    }
    
    template <typename T>
    template <typename F>
    void scmp<T>::consume(F&& functor)
    {
        _queues.for_each(std::forward<F>(functor));
    }
}
