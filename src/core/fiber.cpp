#include "core/fiber.hpp"


namespace np
{
    namespace detail
    {
#if defined(NP_DETAIL_USE_NAKED_RESUME)
        // Windows fucks up with the registers when calling the on-top function
        //  which means the entrypoint won't receive the same transfer as the on-top function
        //  In fact, it receives null, completely fucking up the call
        // Also, we can skip and and all wrapping up around the function, which comes in handy to
        //  skip a few instructions
        
        // Thus, we manually make a naked function (as x64 has inline asm disabled)
        static uint8_t naked_resume[] = {
            // RAX = reinterpret_cast<np::fiber*>(transfer.data)
            0x48, 0x8b, 0x41, 0x08,     // mov    rax,QWORD PTR[rcx + 0x8]
            // RAX = fiber->_former_ctx
            0x48, 0x8b, 0x40, 0x08 ,    // mov    rax,QWORD PTR[rax + 0x8]
            // R9 = transfer.fctx
            0x4c, 0x8b, 0x09,           // mov    r9,QWORD PTR[rcx]
            // *RAX (*_former_ctx) = R9
            0x4c, 0x89, 0x08,           // mov    QWORD PTR[rax],r9
            // Done
            0xC3                        // ret
        };
        
        using call_fn = boost::context::detail::transfer_t(*)(boost::context::detail::transfer_t);

        // Alloc memory only once, copy the bytes, and then point to it        
        void* naked_resume_ptr = ([]() {
#if defined(_MSC_VER)
            void* resume_ptr = VirtualAlloc(nullptr, sizeof(naked_resume), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            assert(resume_ptr != nullptr && "Could not alloc memory for fiber naked resume");
#else
            void* resume_ptr = detail::aligned_malloc(page_size(), page_size());
            assert(resume_ptr != nullptr && "Could not alloc memory for fiber naked resume");
            mprotect(resume_ptr, page_size(), PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
            std::memcpy(resume_ptr, naked_resume, sizeof(naked_resume));
            return resume_ptr;
        })();
#endif
    }

    fiber_base::fiber_base() noexcept :
        fiber_base{ "Fibers/%d" }
    {}

    fiber_base::fiber_base(const char* fiber_name) noexcept :
        _ctx(nullptr),
        _former_ctx(nullptr),
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
        boost::context::detail::ontop_fcontext(_former_ctx, &_ctx, &detail::builtin_fiber_yield);
        // boost::context::detail::jump_fcontext(_former_ctx, 0);
    }

    void fiber_base::yield(fiber_base* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::yielded;
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, &detail::builtin_fiber_yield);
    }

    void fiber_base::yield_blocking(fiber_base* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::blocked;
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, &detail::builtin_fiber_yield);
    }
}
