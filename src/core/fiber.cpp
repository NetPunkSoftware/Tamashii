#include "core/fiber.hpp"


namespace np
{
    fiber::fiber() noexcept :
        _id(current_id++),
        _stack(nullptr),
        _stack_size(0),
        _ctx(nullptr),
        _function(nullptr)
    {
        plDeclareVirtualThread(_id, "Fibers/%d", _id);
    }
    
    fiber::fiber(empty_fiber_t)  noexcept :
        fiber{ 524288, nullptr }
    {}

    fiber::fiber(fiber&& other) noexcept :
        fiber()
    {
        std::swap(_id, other._id);
        std::swap(_stack, other._stack);
	    std::swap(_stack_size, other._stack_size);
	    std::swap(_ctx, other._ctx);
	    std::swap(_function, other._function);
    }

    fiber& fiber::operator=(fiber&& other) noexcept
    {
        std::swap(_id, other._id);
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

    inline boost::context::detail::transfer_t builtin_fiber_yield(boost::context::detail::transfer_t transfer)
    {
        auto ctx = (boost::context::detail::fcontext_t*)transfer.data;
        *ctx = transfer.fctx;
        return {nullptr, nullptr};
    }

    void fiber::yield() noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::yielded;
        boost::context::detail::ontop_fcontext(_record.former->_ctx, &_ctx, &builtin_fiber_yield);
        // boost::context::detail::jump_fcontext(_record.former->_ctx, 0);
    }

    void fiber::yield(fiber* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::yielded;
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, &builtin_fiber_yield);
    }
}
