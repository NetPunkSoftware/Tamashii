#pragma once

#include <boost/context/fiber_fcontext.hpp>
#include <boost/context/detail/fcontext.hpp>

#include <cstdlib>
#include <functional>


namespace np
{
    class fiber;

    struct empty_fiber_t
    {};

    namespace detail
    {
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;

        struct record
        {
            np::fiber* former;
            np::fiber* latter;
        };
    }

    class fiber final
    {
        friend class fiber_pool;
        friend inline void detail::builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;

    public:
        fiber() noexcept;
        fiber(empty_fiber_t) noexcept;

        template <typename F>
        fiber(F&& function) noexcept;

        template <typename F>
        fiber(std::size_t stack_size, F&& function) noexcept;

        fiber(const fiber&) = delete;
        fiber& operator=(const fiber&) = delete;

        fiber(fiber&& other) noexcept;
        fiber& operator=(fiber&& other) noexcept;

        ~fiber() noexcept;

        template <typename F>
        void reset(F&& function, uint8_t thread_idx) noexcept;

        inline fiber& resume(fiber* fiber) noexcept;

    private:
        void yield() noexcept;

    private:
        void* _stack;
        uint8_t _thread_idx;
        std::size_t _stack_size;
        boost::context::detail::fcontext_t _ctx;
        boost::context::detail::fcontext_t _fctx;
        std::function<void()> _function;

        detail::record _record;
    };

    template <typename F>
    fiber::fiber(F&& function) noexcept :
        fiber { 524288, std::forward<F>(function) }
    {}

    template <typename F>
    fiber::fiber(std::size_t stack_size, F&& function) noexcept :
        _stack_size(stack_size),
        _fctx(nullptr),
        _function(std::forward<F>(function))
    {
#ifdef _MSC_VER
        _stack = _aligned_malloc(_stack_size, 8);
#else
        _stack = std::aligned_alloc(0, _stack_size);
#endif // _MSC_VER

        _ctx = boost::context::detail::make_fcontext(static_cast<char*>(_stack) + _stack_size, _stack_size, &detail::builtin_fiber_entrypoint);
    }

    template <typename F>
    void fiber::reset(F&& function, uint8_t thread_idx) noexcept
    {
        _thread_idx = thread_idx;
        _function = std::forward<F>(function);
        _ctx = boost::context::detail::make_fcontext(static_cast<char*>(_stack) + _stack_size, _stack_size, &detail::builtin_fiber_entrypoint);
    }

    inline fiber& fiber::resume(fiber* fiber) noexcept
    {
        _record.former = this;
        _record.latter = fiber;
        auto transfer = boost::context::detail::jump_fcontext(fiber->_ctx, &_record);
        return *fiber;
    }

    namespace detail
    {
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept
        {
            auto record = reinterpret_cast<detail::record*>(transfer.data);
            record->former->_ctx = transfer.fctx;
            record->latter->_function();
            //fiber->_ctx = boost::context::detail::jump_fcontext(transfer.fctx, 0).fctx;
            boost::context::detail::jump_fcontext(transfer.fctx, 0);
        }
    }
}
