#pragma once

#include <boost/context/fiber_fcontext.hpp>
#include <boost/context/detail/fcontext.hpp>

#include <cstdlib>
#include <functional>


namespace np
{
    namespace detail
    {
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;
    }


    class fiber final
    {
        friend inline void detail::builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;

    public:
        fiber() noexcept;

        template <typename F>
        fiber(F&& function) noexcept;

        template <typename F>
        fiber(std::size_t stack_size, F&& function) noexcept;

        fiber(const fiber&) = delete;
        fiber& operator=(const fiber&) = delete;

        fiber(fiber&& other);
        fiber& operator=(fiber&& other);

        ~fiber() noexcept;

        inline fiber& resume(fiber* fiber) noexcept;

    private:
        void* _stack;
        std::size_t _stack_size;
        boost::context::detail::fcontext_t _ctx;
        std::function<void()> _function;
    };

    template <typename F>
    fiber::fiber(F&& function) noexcept :
        fiber { 524288, std::forward<F>(function) }
    {}

    template <typename F>
    fiber::fiber(std::size_t stack_size, F&& function) noexcept :
        _stack_size(stack_size),
        _function(std::forward<F>(function))
    {
        _stack = std::aligned_alloc(0, _stack_size);
        _ctx = boost::context::detail::make_fcontext(static_cast<char*>(_stack) + _stack_size, _stack_size, &detail::builtin_fiber_entrypoint);
    }

    inline fiber& fiber::resume(fiber* fiber) noexcept
    {
        auto transfer = boost::context::detail::jump_fcontext(fiber->_ctx, fiber);
        return *fiber;
    }


    namespace detail
    {
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept
        {
            auto fiber = reinterpret_cast<np::fiber*>(transfer.data);
            fiber->_function();
            // fiber->_ctx = boost::context::detail::jump_fcontext(transfer.fctx, 0).fctx;
            boost::context::detail::jump_fcontext(transfer.fctx, 0);
        }
    }
}
