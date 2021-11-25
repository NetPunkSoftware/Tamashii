#include "core/fiber.hpp"


namespace np
{
    fiber::fiber() noexcept :
        fiber { "Fibers/%d" }
    {}

    fiber::fiber(const char* fiber_name) noexcept :
        _id(current_id++),
        _stack(nullptr),
        _stack_size(0),
        _ctx(nullptr),
        _function(nullptr),
        _counter(&detail::dummy_counter)
    {
        plDeclareVirtualThread(_id, fiber_name, _id);
    }
    
    fiber::fiber(empty_fiber_t)  noexcept :
        fiber{ 524288, nullptr }
    {}

    fiber::fiber(std::size_t stack_size, empty_fiber_t)  noexcept :
        fiber{ "Fibers/%d", stack_size, nullptr }
    {}

    fiber::fiber(const char* fiber_name, std::size_t stack_size, empty_fiber_t)   noexcept :
        fiber{ fiber_name, stack_size, nullptr }
    {}

    fiber::fiber(fiber&& other) noexcept :
        fiber()
    {
        std::swap(_id, other._id);
        std::swap(_stack, other._stack);
	    std::swap(_stack_size, other._stack_size);
	    std::swap(_ctx, other._ctx);
	    std::swap(_function, other._function);
        std::swap(_status, other._status);
        std::swap(_execution_status, other._execution_status);
    }

    fiber& fiber::operator=(fiber&& other) noexcept
    {
        std::swap(_id, other._id);
        std::swap(_stack, other._stack);
        std::swap(_stack_size, other._stack_size);
        std::swap(_ctx, other._ctx);
        std::swap(_function, other._function);
        std::swap(_status, other._status);
        std::swap(_execution_status, other._execution_status);
        return *this;
    }
    
    fiber::~fiber() noexcept
    {
#if defined(NP_DETAIL_USING_FIBER_GUARD_STACK)
        detail::memory_guard_release(_stack, page_size);
        detail::memory_guard_release(static_cast<char*>(_stack) + page_size + _stack_size, page_size);
#endif
        detail::aligned_free(_stack);
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

    void fiber::yield_blocking(fiber* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::blocked;
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, &builtin_fiber_yield);
    }
}
