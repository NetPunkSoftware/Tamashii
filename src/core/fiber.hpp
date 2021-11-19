#pragma once

#include <boost/context/fiber_fcontext.hpp>
#include <boost/context/detail/fcontext.hpp>

#include <palanteer.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <functional>


namespace np
{
    class fiber;

    struct empty_fiber_t
    {};

    enum class fiber_status : uint8_t
    {
        uninitialized,
        initialized,
        running,
        ended,
        yielded
    };

    namespace detail
    {
        inline boost::context::detail::transfer_t builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
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
        friend inline boost::context::detail::transfer_t detail::builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
        friend inline void detail::builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;

        static inline uint32_t current_id = 0;

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
        void yield() noexcept;
        void yield(fiber* to) noexcept;

        inline fiber_status status() const noexcept;

    private:
        uint32_t _id;
        void* _stack;
        uint8_t _thread_idx;
        std::size_t _stack_size;
        boost::context::detail::fcontext_t _ctx;
        std::function<void()> _function;
        fiber_status _status;

        detail::record _record;
    };

    template <typename F>
    fiber::fiber(F&& function) noexcept :
        fiber { 524288, std::forward<F>(function) }
    {}

    template <typename F>
    fiber::fiber(std::size_t stack_size, F&& function) noexcept :
        _id(current_id++),
        _stack_size(stack_size),
        _function(std::forward<F>(function)),
        _status(fiber_status::initialized)
    {
        plDeclareVirtualThread(_id, "Fibers/%d", _id);

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
        _status = fiber_status::initialized;
    }

    inline fiber& fiber::resume(fiber* fiber) noexcept
    {
        fiber->_record.former = this;
        fiber->_record.latter = fiber;
        fiber->_status = fiber_status::running;
        boost::context::detail::ontop_fcontext(fiber->_ctx, &fiber->_record, &detail::builtin_fiber_resume);
        // auto transfer = boost::context::detail::jump_fcontext(fiber->_ctx, &fiber->_record);
        return *fiber;
    }

    inline fiber_status fiber::status() const noexcept
    {
        return _status;
    }

    namespace detail
    {
        inline boost::context::detail::transfer_t builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept
        {
            auto record = reinterpret_cast<detail::record*>(transfer.data);
            record->former->_ctx = transfer.fctx;
            return { nullptr, nullptr };
        }
        
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept
        {
            auto record = reinterpret_cast<detail::record*>(transfer.data);
            record->latter->_function();
            record->latter->_status = fiber_status::ended;
            plDetachVirtualThread(false);
            //fiber->_ctx = boost::context::detail::jump_fcontext(transfer.fctx, 0).fctx;
            boost::context::detail::jump_fcontext(record->former->_ctx, 0);
        }
    }
}
