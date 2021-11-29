#include "core/fiber.hpp"


namespace np
{
    namespace detail
    {
#if defined(_MSC_VER)
        // Windows fucks up with the registers when calling the on-top function
        //  which means the entrypoint won't receive the same transfer as the on-top function
        // In fact, it receives null, completely fucking up the call

        // Thus, we manually make a naked function (as x64 has inline asm disabled)
        static uint8_t naked_resume[] = {
            // RAX = reinterpret_cast<detail::record*>(transfer.data)
            0x48, 0x8b, 0x41, 0x08,     // mov    rax,QWORD PTR[rcx + 0x8]
            // RAX = record->former
            0x48, 0x8b, 0x00,           // mov    rax,QWORD PTR[rax]
            // R9 = transfer.fctx
            0x4c, 0x8b, 0x09,           // mov    r9,QWORD PTR[rcx]
            // RAX->_ctx = R9
            0x4c, 0x89, 0x48, 0x18,     // mov    QWORD PTR[rax + 0x18],r9
            // Done
            0xC3                        // ret
        };

        // Alloc memory only once, copy the bytes, and then point to it
        using call_fn = boost::context::detail::transfer_t(*)(boost::context::detail::transfer_t);
        void* naked_resume_ptr = ([]() {
            void* resume_ptr = VirtualAlloc(nullptr, sizeof(naked_resume), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            assert(resume_ptr != nullptr && "Could not alloc memory for fiber naked resume");
            std::memcpy(resume_ptr, naked_resume, sizeof(naked_resume));
            return resume_ptr;
        })();
#endif
    }
}

namespace np
{
    fiber::fiber() noexcept :
        fiber { "Fibers/%d" }
    {}

    fiber::fiber(const char* fiber_name) noexcept :
        _id(current_id++),
        _stack(nullptr),
        _stack_size(0),
        _ctx(nullptr),
        _function(nullptr),
        _counter(&detail::dummy_counter)
    {
        plDeclareVirtualThread(_id, fiber_name, _id);
    }
    
    fiber::fiber(empty_fiber_t)  noexcept :
        fiber{ 524288, nullptr }
    {}

    fiber::fiber(std::size_t stack_size, empty_fiber_t)  noexcept :
        fiber{ "Fibers/%d", stack_size, nullptr }
    {}

    fiber::fiber(const char* fiber_name, std::size_t stack_size, empty_fiber_t)   noexcept :
        fiber{ fiber_name, stack_size, nullptr }
    {}

    fiber::fiber(fiber&& other) noexcept :
        fiber()
    {
        std::swap(_id, other._id);
        std::swap(_stack, other._stack);
	    std::swap(_stack_size, other._stack_size);
	    std::swap(_ctx, other._ctx);
	    std::swap(_function, other._function);
        std::swap(_status, other._status);
        _execution_status = other._execution_status.load(std::memory_order_release); // TODO(gpascualg): Mem order
    }

    fiber& fiber::operator=(fiber&& other) noexcept
    {
        std::swap(_id, other._id);
        std::swap(_stack, other._stack);
        std::swap(_stack_size, other._stack_size);
        std::swap(_ctx, other._ctx);
        std::swap(_function, other._function);
        std::swap(_status, other._status);
        _execution_status = other._execution_status.load(std::memory_order_release); // TODO(gpascualg): Mem order
        return *this;
    }
    
    fiber::~fiber() noexcept
    {
#if defined(NP_DETAIL_USING_FIBER_GUARD_STACK)
        detail::memory_guard_release(_stack, page_size);
        detail::memory_guard_release(static_cast<char*>(_stack) + page_size + _stack_size, page_size);
#endif
        detail::aligned_free(_stack);
    }

    inline boost::context::detail::transfer_t builtin_fiber_yield(boost::context::detail::transfer_t transfer)
    {
        auto ctx = (boost::context::detail::fcontext_t*)transfer.data;
        *ctx = transfer.fctx;
        return {nullptr, nullptr};
    }

    void fiber::yield() noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::yielded;
        boost::context::detail::ontop_fcontext(_record.former->_ctx, &_ctx, &builtin_fiber_yield);
        // boost::context::detail::jump_fcontext(_record.former->_ctx, 0);
    }

    void fiber::yield(fiber* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::yielded;
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, &builtin_fiber_yield);
    }

    void fiber::yield_blocking(fiber* to) noexcept
    {
        plDetachVirtualThread(true);
        _status = fiber_status::blocked;
        boost::context::detail::ontop_fcontext(to->_ctx, &_ctx, &builtin_fiber_yield);
    }
}
