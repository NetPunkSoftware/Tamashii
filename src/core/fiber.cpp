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
}
