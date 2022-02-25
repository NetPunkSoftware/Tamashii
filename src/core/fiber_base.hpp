#pragma once

#include "core/detail/fiber.hpp"
#include "synchronization/counter.hpp"
#include "utils/badge.hpp"

#include <fcontext/fcontext.h>

#include <cassert>
#include <inttypes.h>


namespace np
{
    namespace detail
    {
        inline fcontext_transfer_t builtin_fiber_resume(fcontext_transfer_t transfer) noexcept;
        inline fcontext_transfer_t builtin_fiber_yield(fcontext_transfer_t transfer) noexcept;
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
        friend inline fcontext_transfer_t detail::builtin_fiber_resume(fcontext_transfer_t transfer) noexcept;
        friend inline fcontext_transfer_t detail::builtin_fiber_yield(fcontext_transfer_t transfer) noexcept;

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

        fiber_base(const fiber_base&) = delete;
        fiber_base& operator=(const fiber_base&) = delete;

        fiber_base(fiber_base&& other) noexcept;
        fiber_base& operator=(fiber_base&& other) noexcept;

        inline fiber_status status() const noexcept;
        inline np::counter* counter() noexcept;

        inline fiber_pool_base* get_fiber_pool() const noexcept;
        inline uint32_t id() const noexcept;
        inline void execution_status(::badge<fiber_pool_base>, fiber_execution_status status) noexcept;
        inline fiber_execution_status execution_status(::badge<fiber_pool_base>) noexcept;

        inline fiber_base& resume(fiber_base* fiber) noexcept;
        void yield() noexcept;
        void yield(fiber_base* to) noexcept;

        // Explicit methods
        inline void set_fiber_pool(::badge<fiber_pool_base>, fiber_pool_base* fiber_pool) noexcept;
        inline fiber_base& resume(fiber_pool_base* fiber_pool, fiber_base* fiber) noexcept;

    protected:
        void yield_blocking(fiber_base* to) noexcept;

        inline constexpr auto badge()
        {
            return ::badge<fiber_base>{};
        }

        // Only fiber may destroy fiber_base
    protected:
        ~fiber_base() noexcept;

    protected:
        // Context switching information
        fcontext_t _ctx;
        fcontext_t* _former_ctx;
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

    inline fiber_pool_base* fiber_base::get_fiber_pool() const noexcept
    {
        assert(_fiber_pool != nullptr && "Fiber has no fiber_pool");
        return _fiber_pool;
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

#if defined(NP_DETAIL_USE_NAKED_CONTEXT)
        detail::call_fn fiber_resume = (detail::call_fn)detail::naked_resume_ptr;
#else
        detail::call_fn fiber_resume = &detail::builtin_fiber_resume;
#endif

        ontop_fcontext(fiber->_ctx, fiber, fiber_resume);
        return *fiber;
    }

    inline void fiber_base::set_fiber_pool(::badge<fiber_pool_base>, fiber_pool_base* fiber_pool) noexcept
    {
        _fiber_pool = fiber_pool;
    }

    namespace detail
    {
        inline fcontext_transfer_t builtin_fiber_resume(fcontext_transfer_t transfer) noexcept
        {
            auto fiber = reinterpret_cast<np::fiber_base*>(transfer.data);
            *fiber->_former_ctx = transfer.ctx;
            return { nullptr, nullptr };
        }

        inline fcontext_transfer_t builtin_fiber_yield(fcontext_transfer_t transfer) noexcept
        {
            auto ctx = (fcontext_t*)transfer.data;
            *ctx = transfer.ctx;
            return { nullptr, nullptr };
        }
    }
}
