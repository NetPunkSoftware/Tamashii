#include "core/fiber.hpp"

namespace np
{
    fiber::fiber() noexcept :
        _stack(nullptr),
        _stack_size(0),
        _ctx(nullptr),
        _function(nullptr)
    {}

    fiber::fiber(fiber&& other) :
        fiber()
    {
        std::swap(_stack, other._stack);
	    std::swap(_stack_size, other._stack_size);
	    std::swap(_ctx, other._ctx);
	    std::swap(_function, other._function);
    }

    fiber& fiber::operator=(fiber&& other)
    {
        std::swap(_stack, other._stack);
	    std::swap(_stack_size, other._stack_size);
	    std::swap(_ctx, other._ctx);
	    std::swap(_function, other._function);
        return *this;
    }
    
    fiber::~fiber() noexcept
    {
        std::free(_stack);
    }
}
