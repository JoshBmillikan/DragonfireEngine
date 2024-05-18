//
// Created by josh on 10/17/23.
//

#include "frame_allocator.h"
#include <mutex>
#include <sanitizer/asan_interface.h>
#include <spdlog/spdlog.h>

namespace dragonfire {
static constexpr std::size_t MAX_SIZE = 1 << 21;// 2mb
static constexpr std::size_t BUFFER_COUNT = 2;

static struct MemoryBuffer {
    char memory[MAX_SIZE]{};
    std::size_t offset = 0;
} BUFFERS[BUFFER_COUNT], *CURRENT_BUFFER = &BUFFERS[0];

static std::uint64_t SWAP_COUNT;

static std::mutex MUTEX;

void* frameAllocator::alloc(const std::size_t size) noexcept
{
    if (size == 0)
        return nullptr;
    std::unique_lock lock(MUTEX);
    if (CURRENT_BUFFER->offset + size < MAX_SIZE) {
        void* ptr = &CURRENT_BUFFER->memory[CURRENT_BUFFER->offset];
        CURRENT_BUFFER->offset += size;
        ASAN_UNPOISON_MEMORY_REGION(ptr, size);
        SPDLOG_TRACE("Allocated per-frame memory block of size {}", size);
        return ptr;
    }
    spdlog::warn("Per-Frame memory pool {} exhausted", SWAP_COUNT % BUFFER_COUNT);
    return nullptr;
}

void frameAllocator::nextFrame() noexcept
{
    std::unique_lock lock(MUTEX);
    CURRENT_BUFFER = &BUFFERS[++SWAP_COUNT % BUFFER_COUNT];
    CURRENT_BUFFER->offset = 0;
    ASAN_POISON_MEMORY_REGION(CURRENT_BUFFER->memory, MAX_SIZE);
}

bool frameAllocator::freeLast(const void* ptr, const std::size_t size) noexcept
{
    std::unique_lock lock(MUTEX);
    if (CURRENT_BUFFER->offset < size)
        return false;
    const void* ptr2 = &CURRENT_BUFFER->memory[CURRENT_BUFFER->offset - size];
    if (ptr == ptr2) {
        CURRENT_BUFFER->offset -= size;
        ASAN_POISON_MEMORY_REGION(ptr2, size);
        SPDLOG_TRACE("Freed per-frame memory block of size {}", size);
        return true;
    }
    return false;
}

}// namespace dragonfire