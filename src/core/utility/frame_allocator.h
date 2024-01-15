//
// Created by josh on 10/17/23.
//

#pragma once
#include <cstdint>
#include <exception>

namespace dragonfire {

namespace frameAllocator {
    /***
     * @brief Allocates temporary per-frame memory
     * @param size size of the allocation
     * @return ptr to the allocation
     */
    void* alloc(std::size_t size) noexcept;
    /***
     * @brief Clears the per-frame allocator, all of the previous allocations are invalidated by
     * this call
     */
    void nextFrame() noexcept;
    /***
     * @brief Attempt to free a memory block allocated from the per-frame pool.
     * This is only possible if the memory block is the last allocation.
     * If it is not, this is a no-op.
     * @param ptr pointer to the memory block to free
     * @param size size of the memory block
     * @return true if the block was freed, false if it was not a pointer to the last allocation
     */
    bool freeLast(void* ptr, std::size_t size) noexcept;
}// namespace frameAllocator

template<typename T>
class FrameAllocator {

public:
#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-identifier-naming"
    using value_type = T;                     // NOLINT(*-identifier-naming)
    typedef value_type* pointer;              // NOLINT(*-identifier-naming)
    typedef const value_type* const_pointer;  // NOLINT(*-identifier-naming)
    typedef value_type& reference;            // NOLINT(*-identifier-naming)
    typedef const value_type& const_reference;// NOLINT(*-identifier-naming)
    typedef std::size_t size_type;            // NOLINT(*-identifier-naming)
    typedef std::ptrdiff_t difference_type;   // NOLINT(*-identifier-naming)
#pragma clang diagnostic pop

    FrameAllocator() noexcept = default;

    T* allocate(const size_type n) const
    {
        size_type size = n * sizeof(T);
        void* ptr = frameAllocator::alloc(size);
        if (ptr)
            return static_cast<T*>(ptr);
        throw std::bad_alloc();
    }

    void deallocate(T* ptr, size_type size) const { frameAllocator::freeLast(ptr, size); }

    template<class U>
    FrameAllocator(const FrameAllocator<U>&)
    {
    }

    template<class U>
    FrameAllocator(const FrameAllocator<U>&&)
    {
    }

    template<class U>
    bool operator==(const FrameAllocator<U>&) const
    {
        return true;
    }

    template<class U>
    bool operator!=(const FrameAllocator<U>&) const
    {
        return false;
    }
};

}// namespace raven