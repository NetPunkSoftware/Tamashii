#pragma once

#include "core/detail/fiber.hpp"
#include "synchronization/counter.hpp"
#include "utils/badge.hpp"

#include <boost/context/fiber_fcontext.hpp>
#include <boost/context/detail/fcontext.hpp>

#include <inttypes.h>


namespace np
{
    namespace detail
    {
        inline boost::context::detail::transfer_t builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
        inline boost::context::detail::transfer_t builtin_fiber_yield(boost::context::detail::transfer_t transfer) noexcept;
    }


    struct empty_fiber_t
    {};

    enum class fiber_status : uint8_t
    {
        uninitialized,
        initialized,
        running,
        ended,
        yielded,
        blocked
    };

    enum class fiber_execution_status : uint8_t
    {
        executing,
        ready
    };


    class fiber_base
    {
        friend class fiber_pool_base;
        template <typename traits> friend class fiber_pool;
        friend inline boost::context::detail::transfer_t detail::builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
        friend inline boost::context::detail::transfer_t detail::builtin_fiber_yield(boost::context::detail::transfer_t transfer) noexcept;

    protected:
        static inline uint32_t current_id = 10000;
        static inline std::size_t page_size = detail::page_size();

    public:
        fiber_base() noexcept;
        fiber_base(const char* fiber_name) noexcept;
        fiber_base(empty_fiber_t) noexcept;
        fiber_base(std::size_t stack_size, empty_fiber_t) noexcept;
        fiber_base(const char* fiber_name, empty_fiber_t) noexcept;
        fiber_base(const char* fiber_name, std::size_t stack_size, empty_fiber_t) noexcept;

        ~fiber_base() noexcept;

        fiber_base(const fiber_base&) = delete;
        fiber_base& operator=(const fiber_base&) = delete;

        fiber_base(fiber_base&& other) noexcept;
        fiber_base& operator=(fiber_base&& other) noexcept;

        inline fiber_status status() const noexcept;
        inline np::counter* counter() noexcept;

        inline uint32_t id() const noexcept;
        inline void execution_status(::badge<fiber_pool_base>, fiber_execution_status status) noexcept;
        inline fiber_execution_status execution_status(::badge<fiber_pool_base>) noexcept;

        inline fiber_base& resume(fiber_base* fiber) noexcept;
        void yield() noexcept;
        void yield(fiber_base* to) noexcept;

        // Explicit methods
        inline fiber_base& resume(fiber_pool_base* fiber_pool, fiber_base* fiber) noexcept;

    protected:
        void yield_blocking(fiber_base* to) noexcept;

        inline constexpr auto badge()
        {
            return ::badge<fiber_base>{};
        }

    protected:
        // Context switching information
        boost::context::detail::fcontext_t _ctx;
        boost::context::detail::fcontext_t* _former_ctx;
        fiber_pool_base* _fiber_pool;

        // Fiber information
        uint32_t _id;
        std::size_t _stack_size;
        fiber_status _status;
        std::atomic<fiber_execution_status> _execution_status;

        // Execution information
        np::counter* _counter;
        void* _stack;
    };


    inline fiber_status fiber_base::status() const noexcept
    {
        return _status;
    }

    inline np::counter* fiber_base::counter() noexcept
    {
        return _counter;
    }

    inline uint32_t fiber_base::id() const noexcept
    {
        return _id;
    }

    inline void fiber_base::execution_status(::badge<fiber_pool_base>, fiber_execution_status status) noexcept
    {
        // TODO(gpascualg): Verify memory order!
        _execution_status.store(status, std::memory_order_release);
    }

    inline fiber_execution_status fiber_base::execution_status(::badge<fiber_pool_base>) noexcept
    {
        return _execution_status.load(std::memory_order_acquire);
    }

    inline fiber_base& fiber_base::resume(fiber_base* fiber) noexcept
    {
        return resume(nullptr, fiber);
    }

    inline fiber_base& fiber_base::resume(fiber_pool_base* fiber_pool, fiber_base* fiber) noexcept
    {
        fiber->_former_ctx = &this->_ctx;
        fiber->_fiber_pool = fiber_pool;
        fiber->_status = fiber_status::running;

#if defined(NP_DETAIL_USE_NAKED_RESUME)
        detail::call_fn fiber_resume = (detail::call_fn)detail::naked_resume_ptr;
#else
        detail::call_fn fiber_resume = &detail::builtin_fiber_resume;
#endif

        boost::context::detail::ontop_fcontext(fiber->_ctx, fiber, fiber_resume);
        return *fiber;
    }


    namespace detail
    {
        inline boost::context::detail::transfer_t builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept
        {
            auto fiber = reinterpret_cast<np::fiber_base*>(transfer.data);
            *fiber->_former_ctx = transfer.fctx;
            return { nullptr, nullptr };
        }

        inline boost::context::detail::transfer_t builtin_fiber_yield(boost::context::detail::transfer_t transfer) noexcept
        {
            auto ctx = (boost::context::detail::fcontext_t*)transfer.data;
            *ctx = transfer.fctx;
            return { nullptr, nullptr };
        }
    }
}