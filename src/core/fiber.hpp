#pragma once

#include "synchronization/counter.hpp"
#include "utils/badge.hpp"

#include <boost/context/fiber_fcontext.hpp>
#include <boost/context/detail/fcontext.hpp>

#include <palanteer.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <functional>


#if defined(NP_GUARD_FIBER_STACK) || !defined(NDEBUG)
#   define NP_DETAIL_USING_FIBER_GUARD_STACK
#   if !defined(_MSC_VER)
#		include <sys/mman.h>
#		include <unistd.h>
#	else
#		define WIN32_LEAN_AND_MEAN
#		define NOMINMAX
#		include <Windows.h>
#	endif
#endif

namespace np
{
    class fiber;
    class fiber_pool_base;

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

    namespace detail
    {
        inline boost::context::detail::transfer_t builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;

        inline std::size_t page_size() noexcept
        {
#if defined(_MSC_VER)
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            return sysInfo.dwPageSize;
#else
            return static_cast<size_t>(getpagesize());
#endif
        }

        inline std::size_t round_up(std::size_t stack_size, std::size_t page_size_or_zero) {
            if (page_size_or_zero == 0)
            {
                return stack_size;
            }

            std::size_t const remainder = stack_size % page_size_or_zero;
            if (remainder == 0) {
                return stack_size;
            }

            return stack_size + page_size_or_zero - remainder;
        }

        inline auto aligned_malloc(std::size_t size, std::size_t alignment)
        {
#ifdef _MSC_VER
            return _aligned_malloc(size, alignment);
#else
            return std::aligned_alloc(alignment, size);
#endif // _MSC_VER
        }

        inline void aligned_free(void* ptr)
        {
#ifdef _MSC_VER
            return _aligned_free(ptr);
#else
            return std::free(ptr);
#endif // _MSC_VER
        }

#if defined(NP_DETAIL_USING_FIBER_GUARD_STACK)
        inline void memory_guard(void* addr, std::size_t len)
        {
#ifdef _MSC_VER
            DWORD prev;
            VirtualProtect(addr, len, PAGE_NOACCESS, &prev);
#else
            mprotect(addr, len, PROT_NONE);
#endif // _MSC_VER
        }

        inline void memory_guard_release(void* addr, std::size_t len)
        {
#ifdef _MSC_VER
            DWORD prev;
            VirtualProtect(addr, len, PAGE_READWRITE, &prev);
#else
            mprotect(addr, len, PROT_READ | PROT_WRITE);
#endif // _MSC_VER
        }
#endif

        // Method to call when resuming fibers
        using call_fn = boost::context::detail::transfer_t(*)(boost::context::detail::transfer_t);
#if defined(_MSC_VER)
        // Alloc memory only once, copy the bytes, and then point to it
        extern void* naked_resume_ptr;
#endif

        inline np::counter dummy_counter(true);

        struct record
        {
            np::fiber* former;
            np::fiber* latter;
        };
    }

    class fiber final
    {
        friend class fiber_pool_base;
        template <typename traits> friend class fiber_pool;
        friend inline boost::context::detail::transfer_t detail::builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
        friend inline void detail::builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;

        static inline uint32_t current_id = 10000;
        static inline std::size_t page_size = detail::page_size();

    public:
        fiber() noexcept;
        fiber(const char* fiber_name) noexcept;
        fiber(empty_fiber_t) noexcept;
        fiber(std::size_t stack_size, empty_fiber_t) noexcept;
        fiber(const char* fiber_name, std::size_t stack_size, empty_fiber_t) noexcept;

        template <typename F>
        fiber(F&& function) noexcept;

        template <typename F>
        fiber(std::size_t stack_size, F&& function) noexcept;

        template <typename F>
        fiber(const char* fiber_name, std::size_t stack_size, F&& function) noexcept;

        fiber(const fiber&) = delete;
        fiber& operator=(const fiber&) = delete;

        fiber(fiber&& other) noexcept;
        fiber& operator=(fiber&& other) noexcept;

        ~fiber() noexcept;

        template <typename F>
        void reset(F&& function) noexcept;

        template <typename F>
        void reset(F&& function, np::counter& counter) noexcept;

        inline fiber& resume(fiber* fiber) noexcept;
        void yield() noexcept;
        void yield(fiber* to) noexcept;

        inline fiber_status status() const noexcept;
        inline np::counter* counter() noexcept;
        
        inline uint32_t id() const noexcept;
        inline void execution_status(badge<fiber_pool_base>, fiber_execution_status status) noexcept;
        inline fiber_execution_status execution_status(badge<fiber_pool_base>) noexcept;

    private:
        void yield_blocking(fiber* to) noexcept;
        inline void execute() noexcept;

    private:
        uint32_t _id;
        void* _stack;
        std::size_t _stack_size;
        boost::context::detail::fcontext_t _ctx;
        std::function<void()> _function;
        fiber_status _status;
        std::atomic<fiber_execution_status> _execution_status;

        // Fiber switching
        np::counter* _counter;
        detail::record _record;
    };

    template <typename F>
    fiber::fiber(F&& function) noexcept :
        fiber{ 524288, std::forward<F>(function) }
    {}

    template <typename F>
    fiber::fiber(std::size_t stack_size, F&& function) noexcept :
        fiber{ "Fibers/%d", stack_size, std::forward<F>(function) }
    {}

    template <typename F>
    fiber::fiber(const char* fiber_name, std::size_t stack_size, F&& function) noexcept :
        _id(current_id++),
#if !defined(NP_DETAIL_USING_FIBER_GUARD_STACK)
        _stack_size(stack_size),
#endif
        _function(std::forward<F>(function)),
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

    template <typename F>
    inline void fiber::reset(F&& function) noexcept
    {
        reset(std::forward<F>(function), detail::dummy_counter);
    }

    template <typename F>
    inline void fiber::reset(F&& function, np::counter& counter) noexcept
    {
        _function = std::forward<F>(function);
        _counter = &counter;
#if defined(NP_DETAIL_USING_FIBER_GUARD_STACK)
        _ctx = boost::context::detail::make_fcontext(static_cast<char*>(_stack) + page_size + _stack_size, _stack_size, &detail::builtin_fiber_entrypoint);
#else
        _ctx = boost::context::detail::make_fcontext(static_cast<char*>(_stack) + _stack_size, _stack_size, &detail::builtin_fiber_entrypoint);
#endif
        _status = fiber_status::initialized;
    }

    inline fiber& fiber::resume(fiber* fiber) noexcept
    {
        fiber->_record.former = this;
        fiber->_record.latter = fiber;
        fiber->_status = fiber_status::running;


#if defined(_MSC_VER)
        detail::call_fn fiber_resume = (detail::call_fn)detail::naked_resume_ptr;
#else
        detail::call_fn fiber_resume = &detail::builtin_fiber_resume;
#endif

        boost::context::detail::ontop_fcontext(fiber->_ctx, &fiber->_record, fiber_resume);
        //boost::context::detail::ontop_fcontext(fiber->_ctx, &fiber->_record, &detail::builtin_fiber_resume);
        // auto transfer = boost::context::detail::jump_fcontext(fiber->_ctx, &fiber->_record);
        return *fiber;
    }

    inline fiber_status fiber::status() const noexcept
    {
        return _status;
    }

    inline np::counter* fiber::counter() noexcept
    {
        return _counter;
    }

    inline uint32_t fiber::id() const noexcept
    {
        return _id;
    }

    inline void fiber::execution_status(badge<fiber_pool_base>, fiber_execution_status status) noexcept
    {
        // TODO(gpascualg): Verify memory order!
        _execution_status.store(status, std::memory_order_release);
    }

    inline fiber_execution_status fiber::execution_status(badge<fiber_pool_base>) noexcept
    {
        return _execution_status.load(std::memory_order_acquire);
    }

    inline void fiber::execute() noexcept
    {
        _function();
        _counter->done({});
        _status = fiber_status::ended;
        plDetachVirtualThread(false);
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
            record->latter->execute();
            //fiber->_ctx = boost::context::detail::jump_fcontext(transfer.fctx, 0).fctx;
            boost::context::detail::jump_fcontext(record->former->_ctx, 0);
        }
    }
}
