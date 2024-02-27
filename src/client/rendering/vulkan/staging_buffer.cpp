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

StagingBuffer::StagingBuffer(StagingBuffer&& other) noexcept
    : stagingBuffer(std::move(other.stagingBuffer)), allocator(other.allocator),
      memoryPropertyFlags(other.memoryPropertyFlags)
{
}

StagingBuffer& StagingBuffer::operator=(StagingBuffer&& other) noexcept
{
    if (this == &other)
        return *this;
    stagingBuffer = std::move(other.stagingBuffer);
    allocator = other.allocator;
    memoryPropertyFlags = other.memoryPropertyFlags;
    return *this;
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

vk::Fence StagingBuffer::copy(
    const vk::Buffer dst,
    const vk::CommandBuffer cmd,
    const vk::Queue queue,
    const vk::Device device,
    const vk::DeviceSize size,
    const vk::DeviceSize dstOffset,
    const vk::DeviceSize srcOffset
) const
{
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(beginInfo);

    vk::BufferCopy cpy;
    cpy.size = size, cpy.dstOffset = dstOffset;
    cpy.srcOffset = srcOffset;

    cmd.copyBuffer(stagingBuffer, dst, cpy);

    vk::SubmitInfo submitInfo{};
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.commandBufferCount = 1;
    cmd.end();

    const vk::Fence fence = device.createFence(vk::FenceCreateInfo());
    try {
        queue.submit(submitInfo, fence);
    }
    catch (...) {
        device.destroy(fence);
        throw;
    }
    return fence;
}

void StagingBuffer::clearStagingBuffer()
{
    stagingBuffer.destroy();
}

}// namespace dragonfire::vulkan
