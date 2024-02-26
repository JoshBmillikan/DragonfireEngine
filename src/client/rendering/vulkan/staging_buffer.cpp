//
// Created by josh on 2/26/24.
//

#include "staging_buffer.h"

namespace dragonfire::vulkan {

StagingBuffer::StagingBuffer(GpuAllocator& allocator, const vk::DeviceSize initialSize, const bool coherent)
    : allocator(allocator)
{
    memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (coherent)
        memoryPropertyFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (initialSize > 0) {
        vk::BufferCreateInfo bufInfo{};
        bufInfo.size = initialSize;
        bufInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
        bufInfo.sharingMode = vk::SharingMode::eExclusive;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.priority = 1.0f;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.requiredFlags = memoryPropertyFlags;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        stagingBuffer = allocator.allocate(bufInfo, allocInfo);
    }
}

void* StagingBuffer::getStagingPtr(const vk::DeviceSize size)
{
    if (stagingBuffer.getInfo().size < size) {
        vk::BufferCreateInfo createInfo{};
        createInfo.size = size;
        createInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.sharingMode = vk::SharingMode::eExclusive;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.priority = 1.0f;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.requiredFlags = memoryPropertyFlags;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        stagingBuffer = allocator.allocate(createInfo, allocInfo);
    }
    return stagingBuffer.getInfo().pMappedData;
}
}// namespace dragonfire::vulkan
