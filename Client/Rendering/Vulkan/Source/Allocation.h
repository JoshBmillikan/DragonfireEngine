//
// Created by josh on 5/20/22.
//

#pragma once

#include <vk_mem_alloc.h>

namespace dragonfire::rendering {

struct Allocation {
    static VmaAllocator allocator;
    static void initAllocator(
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            const vk::DispatchLoaderDynamic& loader = VULKAN_HPP_DEFAULT_DISPATCHER
    );
};

}   // namespace dragonfire::rendering