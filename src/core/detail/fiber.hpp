#pragma once

#include "synchronization/counter.hpp"

#include <fcontext/fcontext.h>

#include <cstdlib>
#include <functional>


#if defined(NP_GUARD_FIBER_STACK) || !defined(NDEBUG)
#   define NP_DETAIL_USING_FIBER_GUARD_STACK
#   if !defined(_MSC_VER)
#		include <sys/mman.h>
#		include <unistd.h>
#	else
#               ifndef WIN32_LEAN_AND_MEAN
#    		        define WIN32_LEAN_AND_MEAN
#               endif
#               ifndef NOMINMAX
#       		define NOMINMAX
#               endif
#		include <Windows.h>
#	endif
#endif

#if !defined(NP_DETAIL_USE_NAKED_CONTEXT) //&& defined(_MSC_VER)
    #define NP_DETAIL_USE_NAKED_CONTEXT
#endif

namespace np
{
    class fiber_pool_base;

    namespace detail
    {
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

        template<typename T, typename U> constexpr size_t offset_of(U T::*member)
        {
            return (char*)&((T*)nullptr->*member) - (char*)nullptr;
        }

        // Method to call when resuming fibers
        using call_fn = fcontext_transfer_t(*)(fcontext_transfer_t);
#if defined(NP_DETAIL_USE_NAKED_CONTEXT)
        // Alloc memory only once, copy the bytes, and then point to it
        extern void* naked_resume_ptr;
        extern void* naked_yield_ptr;
#endif

        inline np::counter dummy_counter(true);
    }
}
