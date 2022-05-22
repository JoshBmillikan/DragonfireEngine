//
// Created by josh on 5/20/22.
//

#pragma once

#include <vk_mem_alloc.h>

namespace dragonfire::rendering {

class Allocation {
protected:
    static VmaAllocator allocator;
    VmaAllocation allocation;
    VmaAllocationInfo info;
public:
    Allocation() = delete;
    virtual ~Allocation() noexcept = default;

    /// \brief Initializes the global vma allocator
    /// \param instance the vulkan instance handle
    /// \param physicalDevice the physical device handle
    /// \param device the logical device handle
    /// \param loader the dispatch loader to get vulkan functions from
    static void initAllocator(
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            const vk::DispatchLoaderDynamic& loader = VULKAN_HPP_DEFAULT_DISPATCHER
    );

    /// Destroys the global vma allocator
    inline static void destroyAllocator() noexcept {
        vmaDestroyAllocator(Allocation::allocator);
        Allocation::allocator = nullptr;
        spdlog::info("Destroyed vulkan allocator");
    }
};

class Buffer : public Allocation {
    vk::Buffer buffer;

public:
    ~Buffer() noexcept override { vmaDestroyBuffer(allocator, buffer, allocation); };
};

class Image : public Allocation {
    vk::Image image;

public:
    ~Image() noexcept override { vmaDestroyImage(allocator, image, allocation); }
};
}   // namespace dragonfire::rendering