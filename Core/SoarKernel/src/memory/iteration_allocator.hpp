// Copyright (C) 2015-2016 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef FOONATHAN_MEMORY_ITERATION_ALLOCATOR_HPP_INCLUDED
#define FOONATHAN_MEMORY_ITERATION_ALLOCATOR_HPP_INCLUDED

/// \file
/// Class template \ref foonathan::memory::iteration_allocator.

#include "detail/debug_helpers.hpp"
#include "detail/memory_stack.hpp"
#include "default_allocator.hpp"
#include "error.hpp"
#include "memory_arena.hpp"

namespace foonathan
{
    namespace memory
    {
        namespace detail
        {
            template <class BlockOrRawAllocator>
            using iteration_block_allocator =
                make_block_allocator_t<BlockOrRawAllocator, fixed_block_allocator>;

            struct iteration_allocator_leak_handler
            {
                void operator()(std::ptrdiff_t amount);
            };
        } // namespace detail

        /// A stateful \concept{concept_rawallocator,RawAllocator} that is designed for allocations in a loop.
        /// It uses `N` stacks for the allocation, one of them is always active.
        /// Allocation uses the currently active stack.
        /// Calling \ref iteration_allocator::next_iteration() at the end of the loop,
        /// will make the next stack active for allocation,
        /// effectively releasing all of its memory.
        /// Any memory allocated will thus be usable for `N` iterations of the loop.
        /// This type of allocator is a generalization of the double frame allocator.
        /// \ingroup memory allocator
        template <std::size_t N, class BlockOrRawAllocator = default_allocator>
        class iteration_allocator
            : FOONATHAN_EBO(make_block_allocator_t<BlockOrRawAllocator, fixed_block_allocator>),
              FOONATHAN_EBO(detail::default_leak_checker<detail::iteration_allocator_leak_handler>)
        {
            using leak_checker =
                detail::default_leak_checker<detail::iteration_allocator_leak_handler>;

        public:
            using allocator_type = detail::iteration_block_allocator<BlockOrRawAllocator>;

            /// \effects Creates it with a given initial block size and and other constructor arguments for the \concept{concept_blockallocator,BlockAllocator}.
            /// It will allocate the first (and only) block and evenly divide it on all the stacks it uses.
            template <typename... Args>
            explicit iteration_allocator(std::size_t block_size, Args&&... args)
            : allocator_type(block_size, detail::forward<Args>(args)...), cur_(0u)
            {
                block_         = get_allocator().allocate_block();
                auto cur       = static_cast<char*>(block_.memory);
                auto size_each = block_.size / N;
                for (auto i = 0u; i != N; ++i)
                {
                    stacks_[i] = detail::fixed_memory_stack(cur);
                    cur += size_each;
                }
            }

            /// \effects Allocates a memory block of given size and alignment.
            /// It simply moves the top marker of the currently active stack.
            /// \returns A \concept{concept_node,node} with given size and alignment.
            /// \throws \ref out_of_fixed_memory if the current stack does not have any memory left.
            /// \requires \c size and \c alignment must be valid.
            void* allocate(std::size_t size, std::size_t alignment)
            {
                auto& stack = stacks_[cur_];

                auto fence  = detail::debug_fence_size;
                auto offset = detail::align_offset(stack.top(), alignment);
                if (!stack.top()
                    || (fence + offset + size + fence > std::size_t(block_end(cur_) - stack.top())))
                    FOONATHAN_THROW(out_of_fixed_memory(info(), size));
                return stack.allocate_unchecked(size, offset);
            }

            /// \effects Allocates a memory block of given size and alignment
            /// similar to \ref allocate().
            /// \returns A \concept{concept_node,node| with given size and alignment
            /// or `nullptr` if the current stack does not have any memory left.
            void* try_allocate(std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                auto& stack = stacks_[cur_];
                return stack.allocate(block_end(cur_), size, alignment);
            }

            /// \effects Goes to the next internal stack.
            /// This will clear the stack whose \ref max_iterations() lifetime has reached,
            /// and use it for all allocations in this iteration.
            /// \note This function should be called at the end of the loop.
            void next_iteration() FOONATHAN_NOEXCEPT
            {
                cur_ = (cur_ + 1) % N;
                stacks_[cur_].unwind(block_start(cur_));
            }

            /// \returns The number of iteration each allocation will live.
            /// This is the template parameter `N`.
            static std::size_t max_iterations() FOONATHAN_NOEXCEPT
            {
                return N;
            }

            /// \returns The index of the current iteration.
            /// This is modulo \ref max_iterations().
            std::size_t cur_iteration() const FOONATHAN_NOEXCEPT
            {
                return cur_;
            }

            /// \returns A reference to the \concept{concept_blockallocator,BlockAllocator} used for managing the memory.
            /// \requires It is undefined behavior to move this allocator out into another object.
            allocator_type& get_allocator() FOONATHAN_NOEXCEPT
            {
                return *this;
            }

            /// \returns The amount of memory remaining in the stack with the given index.
            /// This is the number of bytes that are available for allocation.
            std::size_t capacity_left(std::size_t i) const FOONATHAN_NOEXCEPT
            {
                return std::size_t(block_end(i) - stacks_[i].top());
            }

            /// \returns The amount of memory remaining in the currently active stack.
            std::size_t capacity_left() const FOONATHAN_NOEXCEPT
            {
                return capacity_left(cur_iteration());
            }

        private:
            allocator_info info() const FOONATHAN_NOEXCEPT
            {
                return {FOONATHAN_MEMORY_LOG_PREFIX "::iteration_allocator", this};
            }

            char* block_start(std::size_t i) const FOONATHAN_NOEXCEPT
            {
                FOONATHAN_MEMORY_ASSERT(i <= N);
                auto ptr = static_cast<char*>(block_.memory);
                return ptr + (i * block_.size / N);
            }

            char* block_end(std::size_t i) const FOONATHAN_NOEXCEPT
            {
                FOONATHAN_MEMORY_ASSERT(i < N);
                return block_start(i + 1);
            }

            detail::fixed_memory_stack stacks_[N];
            memory_block               block_;
            std::size_t                cur_;

            friend allocator_traits<iteration_allocator<N, BlockOrRawAllocator>>;
            friend composable_allocator_traits<iteration_allocator<N, BlockOrRawAllocator>>;
        };

        /// An alias for \ref iteration_allocator for two iterations.
        /// \ingroup memory allocator
        template <class BlockOrRawAllocator = default_allocator>
        FOONATHAN_ALIAS_TEMPLATE(double_frame_allocator,
                                 iteration_allocator<2, BlockOrRawAllocator>);

#if FOONATHAN_MEMORY_EXTERN_TEMPLATE
        extern template class iteration_allocator<2>;
#endif

        /// Specialization of the \ref allocator_traits for \ref iteration_allocator.
        /// \note It is not allowed to mix calls through the specialization and through the member functions,
        /// i.e. \ref memory_stack::allocate() and this \c allocate_node().
        /// \ingroup memory allocator
        template <std::size_t N, class BlockAllocator>
        class allocator_traits<iteration_allocator<N, BlockAllocator>>
        {
            using checker = typename iteration_allocator<N, BlockAllocator>::leak_checker;

        public:
            using allocator_type = iteration_allocator<N, BlockAllocator>;
            using is_stateful    = std::true_type;

            /// \returns The result of \ref iteration_allocator::allocate().
            static void* allocate_node(allocator_type& state, std::size_t size,
                                       std::size_t alignment)
            {
                auto mem = state.allocate(size, alignment);
                state.checker::on_allocate(size);
                return mem;
            }

            /// \returns The result of \ref memory_stack::allocate().
            static void* allocate_array(allocator_type& state, std::size_t count, std::size_t size,
                                        std::size_t alignment)
            {
                return allocate_node(state, count * size, alignment);
            }

            /// @{
            /// \effects Does nothing besides bookmarking for leak checking, if that is enabled.
            /// Actual deallocation can only be done via \ref memory_stack::unwind().
            static void deallocate_node(allocator_type& state, void*, std::size_t size,
                                        std::size_t) FOONATHAN_NOEXCEPT
            {
                state.checker::on_deallocate(size);
            }

            static void deallocate_array(allocator_type& state, void* ptr, std::size_t count,
                                         std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                deallocate_node(state, ptr, count * size, alignment);
            }
            /// @}

            /// @{
            /// \returns The maximum size which is \ref iteration_allocator::capacity_left().
            static std::size_t max_node_size(const allocator_type& state) FOONATHAN_NOEXCEPT
            {
                return state.capacity_left();
            }

            static std::size_t max_array_size(const allocator_type& state) FOONATHAN_NOEXCEPT
            {
                return state.capacity_left();
            }
            /// @}

            /// \returns The maximum possible value since there is no alignment restriction
            /// (except indirectly through \ref memory_stack::next_capacity()).
            static std::size_t max_alignment(const allocator_type&) FOONATHAN_NOEXCEPT
            {
                return std::size_t(-1);
            }
        };

        /// Specialization of the \ref composable_allocator_traits for \ref iteration_allocator classes.
        /// \ingroup memory allocator
        template <std::size_t N, class BlockAllocator>
        class composable_allocator_traits<iteration_allocator<N, BlockAllocator>>
        {
        public:
            using allocator_type = iteration_allocator<N, BlockAllocator>;

            /// \returns The result of \ref memory_stack::try_allocate().
            static void* try_allocate_node(allocator_type& state, std::size_t size,
                                           std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                return state.try_allocate(size, alignment);
            }

            /// \returns The result of \ref memory_stack::try_allocate().
            static void* try_allocate_array(allocator_type& state, std::size_t count,
                                            std::size_t size,
                                            std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                return state.try_allocate(count * size, alignment);
            }

            /// @{
            /// \effects Does nothing.
            /// \returns Whether the memory will be deallocated by \ref memory_stack::unwind().
            static bool try_deallocate_node(allocator_type& state, void* ptr, std::size_t,
                                            std::size_t) FOONATHAN_NOEXCEPT
            {
                return state.block_.contains(ptr);
            }

            static bool try_deallocate_array(allocator_type& state, void* ptr, std::size_t count,
                                             std::size_t size,
                                             std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                return try_deallocate_node(state, ptr, count * size, alignment);
            }
            /// @}
        };

#if FOONATHAN_MEMORY_EXTERN_TEMPLATE
        extern template class allocator_traits<iteration_allocator<2>>;
        extern template class composable_allocator_traits<iteration_allocator<2>>;
#endif
    }
} // namespace foonathan::memory

#endif // FOONATHAN_MEMORY_ITERATION_ALLOCATOR_HPP_INCLUDED
