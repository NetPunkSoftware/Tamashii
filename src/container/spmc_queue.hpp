#pragma once


#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>


namespace np 
{
    template <typename T>
    class spmc_queue 
    {
    private:
        class array {
        private:
            typedef std::atomic< T* > atomic_type;
            typedef atomic_type       storage_type; 

            std::size_t         capacity_;
            storage_type    *   storage_;

        public:
            array( std::size_t capacity) :
                capacity_{ capacity },
                storage_{ new storage_type[capacity_] } {
                for ( std::size_t i = 0; i < capacity_; ++i) {
                    ::new ( static_cast< void * >( std::addressof( storage_[i]) ) ) atomic_type{ nullptr };
                }
            }

            ~array() {
                for ( std::size_t i = 0; i < capacity_; ++i) {
                    reinterpret_cast< atomic_type * >( std::addressof( storage_[i]) )->~atomic_type();
                }
                delete [] storage_;
            }

            std::size_t capacity() const noexcept {
                return capacity_;
            }

            void push( std::size_t bottom, T * ctx) noexcept {
                reinterpret_cast< atomic_type * >(
                    std::addressof( storage_[bottom % capacity_]) )
                        ->store( ctx, std::memory_order_relaxed);
            }

            T * pop( std::size_t top) noexcept {
                return reinterpret_cast< atomic_type * >(
                    std::addressof( storage_[top % capacity_]) )
                        ->load( std::memory_order_relaxed);
            }

            array * resize( std::size_t bottom, std::size_t top) {
                std::unique_ptr< array > tmp{ new array{ 2 * capacity_ } };
                for ( std::size_t i = top; i != bottom; ++i) {
                    tmp->push( i, pop( i) );
                }
                return tmp.release();
            }
        };

        std::atomic< std::size_t >     top_{ 0 };
        std::atomic< std::size_t >     bottom_{ 0 };
        std::atomic< array * >         array_;
        std::vector< array * >                                  old_arrays_{};
        char                                                    padding_[cacheline_length];

    public:
        spmc_queue( std::size_t capacity = 4096) :
            array_{ new array{ capacity } } {
            old_arrays_.reserve( 32);
        }

        ~spmc_queue() {
            for ( array * a : old_arrays_) {
                delete a;
            }
            delete array_.load();
        }

        spmc_queue( spmc_queue const&) = delete;
        spmc_queue & operator=( spmc_queue const&) = delete;

        bool empty() const noexcept {
            std::size_t bottom = bottom_.load( std::memory_order_relaxed);
            std::size_t top = top_.load( std::memory_order_relaxed);
            return bottom <= top;
        }

        void push( T * ctx) {
            std::size_t bottom = bottom_.load( std::memory_order_relaxed);
            std::size_t top = top_.load( std::memory_order_acquire);
            array * a = array_.load( std::memory_order_relaxed);
            if ( (a->capacity() - 1) < (bottom - top) ) {
                // queue is full
                // resize
                array * tmp = a->resize( bottom, top);
                old_arrays_.push_back( a);
                std::swap( a, tmp);
                array_.store( a, std::memory_order_relaxed);
            }
            a->push( bottom, ctx);
            std::atomic_thread_fence( std::memory_order_release);
            bottom_.store( bottom + 1, std::memory_order_relaxed);
        }
        
        // Pop can only be called from the same thread that pushes
        T * pop() {
            std::size_t bottom = bottom_.load( std::memory_order_relaxed) - 1;
            array * a = array_.load( std::memory_order_relaxed);
            bottom_.store( bottom, std::memory_order_relaxed);
            std::atomic_thread_fence( std::memory_order_seq_cst);
            std::size_t top = top_.load( std::memory_order_relaxed);
            T * ctx = nullptr;
            if ( top <= bottom) {
                // queue is not empty
                ctx = a->pop( bottom);
                assert( nullptr != ctx);
                if ( top == bottom) {
                    // last element dequeued
                    if ( ! top_.compare_exchange_strong( top, top + 1,
                                                        std::memory_order_seq_cst,
                                                        std::memory_order_relaxed) ) {
                        // lose the race
                        ctx = nullptr;
                    }
                    bottom_.store( bottom + 1, std::memory_order_relaxed);
                }
            } else {
                // queue is empty
                bottom_.store( bottom + 1, std::memory_order_relaxed);
            }
            return ctx;
        }

        T * steal() {
            std::size_t top = top_.load( std::memory_order_acquire);
            std::atomic_thread_fence( std::memory_order_seq_cst);
            std::size_t bottom = bottom_.load( std::memory_order_acquire);
            T * ctx = nullptr;
            if ( top < bottom) {
                // queue is not empty
                array * a = array_.load( std::memory_order_consume);
                ctx = a->pop( top);
                assert( nullptr != ctx);
                // do not steal pinned context (e.g. main-/dispatcher-context)
                // if ( ctx->is_context( type::pinned_context) ) {
                //     return nullptr;
                // }
                if ( ! top_.compare_exchange_strong( top, top + 1,
                                                    std::memory_order_seq_cst,
                                                    std::memory_order_relaxed) ) {
                    // lose the race
                    return nullptr;
                }
            }
            return ctx;
        }
    };

}

#if BOOST_COMP_CLANG
#pragma clang diagnostic pop
#endif
