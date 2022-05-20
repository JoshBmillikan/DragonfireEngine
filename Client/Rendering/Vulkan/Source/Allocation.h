//
// Created by josh on 5/20/22.
//

#pragma once

#include <vk_mem_alloc.h>

namespace dragonfire::rendering {

class Allocation {
protected:
    static VmaAllocator allocator;
public:
    static void initAllocator(
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            const vk::DispatchLoaderDynamic& loader = VULKAN_HPP_DEFAULT_DISPATCHER
    );

    inline static void destroyAllocator() noexcept {
        vmaDestroyAllocator(Allocation::allocator);
        Allocation::allocator = nullptr;
        spdlog::info("Destroy vulkan allocator");
    }
};

}   // namespace dragonfire::rendering