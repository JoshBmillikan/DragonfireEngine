//
// Created by josh on 10/17/23.
//

#include "frame_allocator.h"
#include <mutex>
#include <sanitizer/asan_interface.h>
#include <spdlog/spdlog.h>

namespace dragonfire {
static constexpr std::size_t MAX_SIZE = 1 << 21;// 2mb
static char MEMORY[MAX_SIZE];
static std::size_t OFFSET = 0;
static std::mutex MUTEX;

void* frameAllocator::alloc(std::size_t size) noexcept
{
    if (size == 0)
        return nullptr;
    std::unique_lock lock(MUTEX);
    if (OFFSET + size < MAX_SIZE) {
        void* ptr = &MEMORY[OFFSET];
        OFFSET += size;
        ASAN_UNPOISON_MEMORY_REGION(ptr, size);
        SPDLOG_TRACE("Allocated per-frame memory block of size {}", size);
        return ptr;
    }
    spdlog::warn("Per-Frame memory pool exhausted");
    return nullptr;
}

void frameAllocator::nextFrame() noexcept
{
    std::unique_lock lock(MUTEX);
    OFFSET = 0;
    ASAN_POISON_MEMORY_REGION(MEMORY, MAX_SIZE);
}

bool frameAllocator::freeLast(void* ptr, std::size_t size) noexcept
{
    std::unique_lock lock(MUTEX);
    if (OFFSET < size)
        return false;
    const void* ptr2 = &MEMORY[OFFSET - size];
    if (ptr == ptr2) {
        OFFSET -= size;
        ASAN_POISON_MEMORY_REGION(ptr2, size);
        SPDLOG_TRACE("Freed per-frame memory block of size {}", size);
        return true;
    }
    return false;
}

}// namespace core