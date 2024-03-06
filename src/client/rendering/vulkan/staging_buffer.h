//
// Created by josh on 2/26/24.
//

#pragma once
#include "allocation.h"
#include <vulkan/vulkan_raii.hpp>

namespace dragonfire::vulkan {

class StagingBuffer {
    Buffer stagingBuffer;
    GpuAllocator& allocator;
    VkMemoryPropertyFlags memoryPropertyFlags;

public:
    virtual ~StagingBuffer() = default;

    StagingBuffer(
        GpuAllocator& allocator,
        vk::DeviceSize initialSize = 0,
        bool coherent = true,
        const char* name = "staging buffer"
    );

    [[nodiscard]] const Buffer& getStagingBuffer() const { return stagingBuffer; }

    void flushStagingBuffer() const { stagingBuffer.flush(); };

    StagingBuffer(const StagingBuffer& other) = delete;
    StagingBuffer(StagingBuffer&& other) noexcept;
    StagingBuffer& operator=(const StagingBuffer& other) = delete;
    StagingBuffer& operator=(StagingBuffer&& other) noexcept;

    void* getStagingPtr(vk::DeviceSize size);

    [[nodiscard]] vk::Fence copy(
        vk::Buffer dst,
        vk::CommandBuffer cmd,
        vk::Queue queue,
        vk::Device device,
        vk::DeviceSize size,
        vk::DeviceSize dstOffset = 0,
        vk::DeviceSize srcOffset = 0
    ) const;

    void clearStagingBuffer();
};

}// namespace dragonfire::vulkan
