//
// Created by josh on 2/26/24.
//

#pragma once
#include "allocation.h"

namespace dragonfire::vulkan {

class StagingBuffer {
    Buffer stagingBuffer;
    GpuAllocator& allocator;
    VkMemoryPropertyFlags memoryPropertyFlags;

public:
    virtual ~StagingBuffer() = default;
    StagingBuffer(GpuAllocator& allocator, vk::DeviceSize initialSize = 0, bool coherent = true);

    [[nodiscard]] const Buffer& getStagingBuffer() const { return stagingBuffer; }

    void flushStagingBuffer() const { stagingBuffer.flush(); };

protected:
    void* getStagingPtr(vk::DeviceSize size);
};

}// namespace dragonfire::vulkan

// dragonfire
