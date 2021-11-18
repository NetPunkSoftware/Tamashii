#include "core/fiber.hpp"

namespace np
{
    fiber::fiber() noexcept :
        _stack(nullptr),
        _stack_size(0),
        _ctx(nullptr),
        _fctx(nullptr),
        _function(nullptr)
    {}
    
    fiber::fiber(empty_fiber_t)  noexcept :
        fiber{ 524288, nullptr }
    {}

    fiber::fiber(fiber&& other) noexcept :
        fiber()
    {
        std::swap(_stack, other._stack);
	    std::swap(_stack_size, other._stack_size);
	    std::swap(_ctx, other._ctx);
	    std::swap(_function, other._function);
    }

    fiber& fiber::operator=(fiber&& other) noexcept
    {
        std::swap(_stack, other._stack);
	    std::swap(_stack_size, other._stack_size);
	    std::swap(_ctx, other._ctx);
	    std::swap(_function, other._function);
        return *this;
    }
    
    fiber::~fiber() noexcept
    {
#ifdef _MSC_VER
        _aligned_free(_stack);
#else
        std::free(_stack);
#endif // _MSC_VER
    }

    void fiber::yield() noexcept
    {
        _fctx = boost::context::detail::jump_fcontext(_fctx, 0).fctx;
    }
}
