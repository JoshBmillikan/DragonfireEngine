//
// Created by josh on 5/20/22.
//
#define VMA_IMPLEMENTATION
#include "Allocation.h"
namespace dragonfire::rendering {
VmaAllocator Allocation::allocator = nullptr;
void Allocation::initAllocator(
        vk::Instance instance,
        vk::PhysicalDevice physicalDevice,
        vk::Device device,
        const vk::DispatchLoaderDynamic& loader
) {
    // set vma functions to those loaded by vulkan.hpp
    VmaVulkanFunctions functions;
    functions.vkGetInstanceProcAddr = loader.vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = loader.vkGetDeviceProcAddr;
    functions.vkGetPhysicalDeviceProperties = loader.vkGetPhysicalDeviceProperties;
    functions.vkGetPhysicalDeviceMemoryProperties = loader.vkGetPhysicalDeviceMemoryProperties;
    functions.vkAllocateMemory = loader.vkAllocateMemory;
    functions.vkFreeMemory = loader.vkFreeMemory;
    functions.vkMapMemory = loader.vkMapMemory;
    functions.vkUnmapMemory = loader.vkUnmapMemory;
    functions.vkFlushMappedMemoryRanges = loader.vkFlushMappedMemoryRanges;
    functions.vkInvalidateMappedMemoryRanges = loader.vkInvalidateMappedMemoryRanges;
    functions.vkBindBufferMemory = loader.vkBindBufferMemory;
    functions.vkBindImageMemory = loader.vkBindImageMemory;
    functions.vkGetBufferMemoryRequirements = loader.vkGetBufferMemoryRequirements;
    functions.vkGetImageMemoryRequirements = loader.vkGetImageMemoryRequirements;
    functions.vkCreateBuffer = loader.vkCreateBuffer;
    functions.vkDestroyBuffer = loader.vkDestroyBuffer;
    functions.vkCreateImage = loader.vkCreateImage;
    functions.vkDestroyImage = loader.vkDestroyImage;
    functions.vkCmdCopyBuffer = loader.vkCmdCopyBuffer;
    functions.vkGetBufferMemoryRequirements2KHR = loader.vkGetBufferMemoryRequirements2;
    functions.vkGetImageMemoryRequirements2KHR = loader.vkGetImageMemoryRequirements2;
    functions.vkBindBufferMemory2KHR = loader.vkBindBufferMemory2;
    functions.vkBindImageMemory2KHR = loader.vkBindImageMemory2;
    functions.vkGetPhysicalDeviceMemoryProperties2KHR = loader.vkGetPhysicalDeviceMemoryProperties2;
    functions.vkGetDeviceBufferMemoryRequirements = loader.vkGetDeviceBufferMemoryRequirements;
    functions.vkGetDeviceImageMemoryRequirements = loader.vkGetDeviceImageMemoryRequirements;

    VmaAllocatorCreateInfo createInfo{
            .physicalDevice = physicalDevice,
            .device = device,
            .pVulkanFunctions = &functions,
            .instance = instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    if (vmaCreateAllocator(&createInfo, &Allocation::allocator) != VK_SUCCESS)
        throw std::runtime_error("Failed to create vma allocator");
}
}   // namespace dragonfire::rendering