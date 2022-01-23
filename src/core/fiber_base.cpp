#include "core/fiber_base.hpp"
#include "core/fiber.hpp"

#include <palanteer.h>


namespace np
{
    fiber_base::fiber_base() noexcept :
        fiber_base{ "Fibers/%d" }
    {}

    fiber_base::fiber_base(const char* fiber_name) noexcept :
        _ctx(nullptr),
        _former_ctx(nullptr),
        _fiber_pool(nullptr),
        _id(current_id++),
        _stack_size(0),
        _status(fiber_status::uninitialized),
        _execution_status(fiber_execution_status::ready),
        _counter(&detail::dummy_counter),
        _stack(nullptr)
    {
        plDeclareVirtualThread(_id, fiber_name, _id);
    }

    fiber_base::fiber_base(empty_fiber_t) noexcept :
        fiber_base{ "Fibers/%d", 524288, {} }
    {}

    fiber_base::fiber_base(std::size_t stack_size, empty_fiber_t) noexcept :
        fiber_base{ "Fibers/%d", stack_size, {} }
    {}

    fiber_base::fiber_base(const char* fiber_name, empty_fiber_t) noexcept :
        fiber_base{ fiber_name, 524288, {} }
    {}

    fiber_base::fiber_base(const char* fiber_name, std::size_t stack_size, empty_fiber_t) noexcept :
        _ctx(),
        _former_ctx(nullptr),
        _fiber_pool(nullptr),
        _id(current_id++),
        _stack_size(stack_size),
        _status(fiber_status::initialized),
        _execution_status(fiber_execution_status::ready),
        _counter(&detail::dummy_counter)
    {
        plDeclareVirtualThread(_id, fiber_name, _id);

        #if defined(NP_DETAIL_USING_FIBER_GUARD_STACK)
            _stack_size = detail::round_up(stack_size, page_size);
            _stack = detail::aligned_malloc(page_size + _stack_size + page_size, page_size);
            _ctx = boost::context::detail::make_fcontext(static_cast<char*>(_stack) + page_size + _stack_size, _stack_size, &detail::builtin_fiber_entrypoint);
        
            detail::memory_guard(_stack, page_size);
            detail::memory_guard(static_cast<char*>(_stack) + page_size + _stack_size, page_size);
        #else
            _stack = detail::aligned_malloc(_stack_size, sizeof(uintptr_t));
            _ctx = boost::context::detail::make_fcontext(static_cast<char*>(_stack) + _stack_size, _stack_size, &detail::builtin_fiber_entrypoint);
        #endif
    }

    fiber_base::~fiber_base() noexcept
    {
        #if defined(NP_DETAIL_USING_FIBER_GUARD_STACK)
            detail::memory_guard_release(_stack, page_size);
            detail::memory_guard_release(static_cast<char*>(_stack) + page_size + _stack_size, page_size);
        #endif

        detail::aligned_free(_stack);        
    }

    fiber_base::fiber_base(fiber_base&& other) noexcept :
        fiber_base()
    {
        std::swap(_ctx, other._ctx);
        std::swap(_former_ctx, other._former_ctx);
        std::swap(_fiber_pool, other._fiber_pool);
        std::swap(_id, other._id);
        std::swap(_stack_size, other._stack_size);
        std::swap(_status, other._status);
        _execution_status = other._execution_status.load(std::memory_order_release); // TODO(gpascualg): Mem order
        std::swap(_counter, other._counter);
        std::swap(_stack, other._stack);
    }

    fiber_base& fiber_base::operator=(fiber_base&& other) noexcept
    {
        std::swap(_ctx, other._ctx);
        std::swap(_former_ctx, other._former_ctx);
        std::swap(_fiber_pool, other._fiber_pool);
        std::swap(_id, other._id);
        std::swap(_stack_size, other._stack_size);
        std::swap(_status, other._status);
        _execution_status = other._execution_status.load(std::memory_order_release); // TODO(gpascualg): Mem order
        std::swap(_counter, other._counter);
        std::swap(_stack, other._stack);

        return *this;
    }

    void fiber_base::yield() noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::yielded;

#if defined(NP_DETAIL_USE_NAKED_CONTEXT)
        detail::call_fn fiber_yield = (detail::call_fn)detail::naked_yield_ptr;
#else
        detail::call_fn fiber_yield = &detail::builtin_fiber_yield;
#endif
        boost::context::detail::ontop_fcontext(_former_ctx, &_ctx, fiber_yield);
        // boost::context::detail::jump_fcontext(_former_ctx, 0);
    }

    void fiber_base::yield(fiber_base* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::yielded;
        
#if defined(NP_DETAIL_USE_NAKED_CONTEXT)
        detail::call_fn fiber_yield = (detail::call_fn)detail::naked_yield_ptr;
#else
        detail::call_fn fiber_yield = &detail::builtin_fiber_yield;
#endif
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, fiber_yield);
    }

    void fiber_base::yield_blocking(fiber_base* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::blocked;
        
#if defined(NP_DETAIL_USE_NAKED_CONTEXT)
        detail::call_fn fiber_yield = (detail::call_fn)detail::naked_yield_ptr;
#else
        detail::call_fn fiber_yield = &detail::builtin_fiber_yield;
#endif
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, fiber_yield);
    }
}
