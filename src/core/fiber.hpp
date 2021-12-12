#pragma once

#include "core/fiber_base.hpp"
#include "synchronization/counter.hpp"
#include "utils/badge.hpp"

#include <boost/context/fiber_fcontext.hpp>
#include <boost/context/detail/fcontext.hpp>

#include <inplace_function.h>
#include <palanteer.h>
#include <spdlog/spdlog.h>

#include <cstdlib>


namespace np
{
    namespace detail
    {
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;
    }


    struct default_fiber_traits
    {
        static const uint32_t inplace_function_size = 64;
        static const uint32_t stack_size = 524288;
    };

    template <typename traits = default_fiber_traits>
    class fiber final : public fiber_base
    {
        friend class fiber_pool_base;
        template <typename T> friend class fiber_pool;
        friend inline void detail::builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept;

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
        fiber_base(std::move(static_cast<fiber_base&>(other))),
        _function(nullptr)
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
        _counter->done(badge(), _fiber_pool);
        _status = fiber_status::ended;
        plDetachVirtualThread(false);
    }

    namespace detail
    {
        inline void builtin_fiber_entrypoint(boost::context::detail::transfer_t transfer) noexcept
        {
            // TODO(gpascualg): Casting to default traits should not be a problem, as all allocation has already been done
            //  and data that depends on traits (ie. its size is given by traits)
            auto fiber = reinterpret_cast<np::fiber<default_fiber_traits>*>(transfer.data);
            fiber->execute();
            boost::context::detail::jump_fcontext(*fiber->_former_ctx, 0);
        }
    }
}
