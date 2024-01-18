//
// Created by josh on 1/17/24.
//

#define VMA_IMPLEMENTATION
#include "allocation.h"
#include "core/crash.h"


#include <spdlog/spdlog.h>

namespace dragonfire::vulkan {
void* GpuAllocation::map() const
{
    void* ptr;
    resultCheck(
        static_cast<vk::Result>(vmaMapMemory(allocator, allocation, &ptr)),
        "Failed to map allocation"
    );
    return ptr;
}

void GpuAllocation::unmap() const
{
    vmaUnmapMemory(allocator, allocation);
}

GpuAllocation::GpuAllocation(GpuAllocation&& other) noexcept
    : allocation(other.allocation), allocator(other.allocator), info(other.info)
{
    other.allocator = nullptr;
}

GpuAllocation& GpuAllocation::operator=(GpuAllocation&& other) noexcept
{
    if (this == &other)
        return *this;
    allocator = other.allocator;
    other.allocator = nullptr;
    allocation = other.allocation;
    info = other.info;
    return *this;
}

void Buffer::destroy() noexcept
{
    if (allocator)
        vmaDestroyBuffer(allocator, buffer, allocation);
    allocator = nullptr;
}

Buffer::Buffer(Buffer&& other) noexcept : GpuAllocation(std::move(other)), buffer(other.buffer) {}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (this == &other)
        return *this;
    GpuAllocation::operator=(std::move(other));
    buffer = other.buffer;
    return *this;
}

void Image::destroy() noexcept
{
    if (allocator)
        vmaDestroyImage(allocator, image, allocation);
    allocator = nullptr;
}

Image::Image(Image&& other) noexcept
    : GpuAllocation(std::move(other)), image(other.image), format(other.format), extent(other.extent)
{
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this == &other)
        return *this;
    GpuAllocation::operator=(std::move(other));
    image = other.image;
    format = other.format;
    extent = other.extent;
    return *this;
}

GpuAllocator::GpuAllocator(
    const vk::Instance instance,
    const vk::PhysicalDevice physicalDevice,
    const vk::Device device,
    const VULKAN_HPP_DEFAULT_DISPATCHER_TYPE& ld
) noexcept
{
    VmaVulkanFunctions functions;
    functions.vkGetInstanceProcAddr = ld.vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = ld.vkGetDeviceProcAddr;
    functions.vkGetPhysicalDeviceProperties = ld.vkGetPhysicalDeviceProperties;
    functions.vkGetPhysicalDeviceMemoryProperties = ld.vkGetPhysicalDeviceMemoryProperties;
    functions.vkAllocateMemory = ld.vkAllocateMemory;
    functions.vkFreeMemory = ld.vkFreeMemory;
    functions.vkMapMemory = ld.vkMapMemory;
    functions.vkUnmapMemory = ld.vkUnmapMemory;
    functions.vkFlushMappedMemoryRanges = ld.vkFlushMappedMemoryRanges;
    functions.vkInvalidateMappedMemoryRanges = ld.vkInvalidateMappedMemoryRanges;
    functions.vkBindBufferMemory = ld.vkBindBufferMemory;
    functions.vkBindImageMemory = ld.vkBindImageMemory;
    functions.vkGetBufferMemoryRequirements = ld.vkGetBufferMemoryRequirements;
    functions.vkGetImageMemoryRequirements = ld.vkGetImageMemoryRequirements;
    functions.vkCreateBuffer = ld.vkCreateBuffer;
    functions.vkDestroyBuffer = ld.vkDestroyBuffer;
    functions.vkCreateImage = ld.vkCreateImage;
    functions.vkDestroyImage = ld.vkDestroyImage;
    functions.vkCmdCopyBuffer = ld.vkCmdCopyBuffer;
    functions.vkGetBufferMemoryRequirements2KHR = ld.vkGetBufferMemoryRequirements2;
    functions.vkGetImageMemoryRequirements2KHR = ld.vkGetImageMemoryRequirements2;
    functions.vkBindBufferMemory2KHR = ld.vkBindBufferMemory2;
    functions.vkBindImageMemory2KHR = ld.vkBindImageMemory2;
    functions.vkGetPhysicalDeviceMemoryProperties2KHR = ld.vkGetPhysicalDeviceMemoryProperties2;
    functions.vkGetDeviceBufferMemoryRequirements = ld.vkGetDeviceBufferMemoryRequirements;
    functions.vkGetDeviceImageMemoryRequirements = ld.vkGetDeviceImageMemoryRequirements;

    VmaAllocatorCreateInfo createInfo{};
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = device;
    createInfo.instance = instance;
    createInfo.pVulkanFunctions = &functions;
    createInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_2;

    if (vmaCreateAllocator(&createInfo, &allocator) != VK_SUCCESS)
        crash("Failed to create GPU allocator");
}

Buffer GpuAllocator::allocate(
    const vk::BufferCreateInfo& createInfo,
    const VmaAllocationCreateInfo& allocInfo
) const
{
    Buffer buffer;
    const VkBufferCreateInfo& create = createInfo;
    VkBuffer b;
    VkResult result = vmaCreateBuffer(allocator, &create, &allocInfo, &b, &buffer.allocation, &buffer.info);
    resultCheck(static_cast<vk::Result>(result), "VMA failed to allocate buffer");
    buffer.buffer = b;
    buffer.allocator = allocator;
    return buffer;
}

Image GpuAllocator::allocate(const vk::ImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo)
    const
{
    Image image;
    const VkImageCreateInfo& create = createInfo;
    VkImage i;
    VkResult result = vmaCreateImage(allocator, &create, &allocInfo, &i, &image.allocation, &image.info);
    resultCheck(static_cast<vk::Result>(result), "VMA failed to allocate image");
    image.image = i;
    image.allocator = allocator;
    image.format = createInfo.format;
    image.extent = createInfo.extent;
    return image;
}

void GpuAllocator::destroy()
{
    vmaDestroyAllocator(allocator);
    if (allocator) {
        SPDLOG_LOGGER_DEBUG(spdlog::get("Rendering"), "Vulkan allocator destroyed");
    }
    allocator = nullptr;
}

GpuAllocator::~GpuAllocator()
{
    vmaDestroyAllocator(allocator);
}

}// namespace dragonfire::vulkan