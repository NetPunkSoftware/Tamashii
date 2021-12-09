#pragma once

#include "synchronization/counter.hpp"
#include "utils/badge.hpp"

#include <boost/context/fiber_fcontext.hpp>
#include <boost/context/detail/fcontext.hpp>

#include <inplace_function.h>
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

#if !defined(NP_DETAIL_USE_NAKED_RESUME) && defined(_MSC_VER)
    #define NP_DETAIL_USE_NAKED_RESUME
#endif

namespace np
{
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
        inline boost::context::detail::transfer_t builtin_fiber_yield(boost::context::detail::transfer_t transfer) noexcept;

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

        inline std::size_t round_up(std::size_t stack_size, std::size_t page_size_or_zero)
        {
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
    }

    struct default_fiber_traits
    {
        static const uint32_t inplace_function_size = 64;
        static const uint32_t stack_size = 524288;
    };

    class fiber_base
    {
        friend class fiber_pool_base;
        template <typename traits> friend class fiber_pool;
        friend inline boost::context::detail::transfer_t detail::builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
        friend inline void detail::builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;
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
        inline void execution_status(badge<fiber_pool_base>, fiber_execution_status status) noexcept;
        inline fiber_execution_status execution_status(badge<fiber_pool_base>) noexcept;

        inline fiber_base& resume(fiber_base* fiber) noexcept;
        void yield() noexcept;
        void yield(fiber_base* to) noexcept;

    protected:
        void yield_blocking(fiber_base* to) noexcept;

    protected:
        // Context switching information
        boost::context::detail::fcontext_t _ctx;
        boost::context::detail::fcontext_t* _former_ctx;

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

    inline void fiber_base::execution_status(badge<fiber_pool_base>, fiber_execution_status status) noexcept
    {
        // TODO(gpascualg): Verify memory order!
        _execution_status.store(status, std::memory_order_release);
    }

    inline fiber_execution_status fiber_base::execution_status(badge<fiber_pool_base>) noexcept
    {
        return _execution_status.load(std::memory_order_acquire);
    }

    inline fiber_base& fiber_base::resume(fiber_base* fiber) noexcept
    {
        fiber->_former_ctx = &this->_ctx;
        fiber->_status = fiber_status::running;

#if defined(NP_DETAIL_USE_NAKED_RESUME)
        detail::call_fn fiber_resume = (detail::call_fn)detail::naked_resume_ptr;
#else
        detail::call_fn fiber_resume = &detail::builtin_fiber_resume;
#endif

        boost::context::detail::ontop_fcontext(fiber->_ctx, fiber, fiber_resume);
        //boost::context::detail::ontop_fcontext(fiber->_ctx, &fiber->_record, &detail::builtin_fiber_resume);
        // auto transfer = boost::context::detail::jump_fcontext(fiber->_ctx, &fiber->_record);
        return *fiber;
    }


    template <typename traits = default_fiber_traits>
    class fiber final : public fiber_base
    {
        friend class fiber_pool_base;
        template <typename traits> friend class fiber_pool;
        friend inline boost::context::detail::transfer_t detail::builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept;
        friend inline void detail::builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;
        friend inline boost::context::detail::transfer_t detail::builtin_fiber_yield(boost::context::detail::transfer_t transfer) noexcept;

    public:
        using fiber_base::fiber_base;

        template <typename F>
        fiber(F&& function) noexcept;

        template <typename F>
        fiber(std::size_t stack_size, F&& function) noexcept;

        template <typename F>
        fiber(const char* fiber_name, F&& function) noexcept;

        template <typename F>
        fiber(const char* fiber_name, std::size_t stack_size, F&& function) noexcept;

        fiber(const fiber&) = delete;
        fiber<traits>& operator=(const fiber<traits>&) = delete;

        fiber(fiber&& other) noexcept;
        fiber<traits>& operator=(fiber<traits>&& other) noexcept;

        template <typename F>
        void reset(F&& function) noexcept;

        template <typename F>
        void reset(F&& function, np::counter& counter) noexcept;

    private:
        inline void execute() noexcept;

    private:
        stdext::inplace_function<void(), traits::inplace_function_size> _function;
    };

    
    template <typename traits>
    template <typename F>
    fiber<traits>::fiber(F&& function) noexcept :
        fiber{ "Fibers/%d", std::forward<F>(function) }
    {}

    template <typename traits>
    template <typename F>
    fiber<traits>::fiber(std::size_t stack_size, F && function) noexcept :
        fiber{ "Fibers/%d", stack_size, std::forward<F>(function) }
    {}

    template <typename traits>
    template <typename F>
    fiber<traits>::fiber(const char* fiber_name, F&& function) noexcept :
        fiber{ "Fibers/%d", 524288, std::forward<F>(function) }
    {}

    template <typename traits>
    template <typename F>
    fiber<traits>::fiber(const char* fiber_name, std::size_t stack_size, F&& function) noexcept :
        fiber_base{ fiber_name, stack_size, empty_fiber_t{} },
        _function(std::forward<F>(function))
    {}

    template <typename traits>
    fiber<traits>::fiber(fiber<traits>&& other) noexcept :
        fiber(),
        fiber_base(std::move(static_cast<fiber_base&>(other)))
    {
        std::swap(_function, other._function);
    }

    template <typename traits>
    fiber<traits>& fiber<traits>::operator=(fiber<traits>&& other) noexcept
    {
        static_cast<fiber_base&>(*this) = std::move(static_cast<fiber_base&>(other));
        std::swap(_function, other._function);
        return *this;
    }

    template <typename traits>
    template <typename F>
    inline void fiber<traits>::reset(F&& function) noexcept
    {
        reset(std::forward<F>(function), detail::dummy_counter);
    }

    template <typename traits>
    template <typename F>
    inline void fiber<traits>::reset(F&& function, np::counter& counter) noexcept
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

    template <typename traits>
    inline void fiber<traits>::execute() noexcept
    {
        _function();
        _counter->template done<traits>({});
        _status = fiber_status::ended;
        plDetachVirtualThread(false);
    }

    namespace detail
    {
        inline boost::context::detail::transfer_t builtin_fiber_resume(boost::context::detail::transfer_t transfer) noexcept
        {
            auto fiber = reinterpret_cast<np::fiber_base*>(transfer.data);
            *fiber->_former_ctx = transfer.fctx;
            return { nullptr, nullptr };
        }
        
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept
        {
            // TODO(gpascualg): Casting to default traits should not be a problem, as all allocation has already been done
            //  and data that depends on traits (ie. its size is given by traits)
            auto fiber = reinterpret_cast<np::fiber<default_fiber_traits>*>(transfer.data);
            fiber->execute();
            //fiber->_ctx = boost::context::detail::jump_fcontext(transfer.fctx, 0).fctx;
            boost::context::detail::jump_fcontext(*fiber->_former_ctx, 0);
        }

        inline boost::context::detail::transfer_t builtin_fiber_yield(boost::context::detail::transfer_t transfer) noexcept
        {
            auto ctx = (boost::context::detail::fcontext_t*)transfer.data;
            *ctx = transfer.fctx;
            return { nullptr, nullptr };
        }
    }
}
