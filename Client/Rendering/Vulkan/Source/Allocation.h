//
// Created by josh on 5/20/22.
//

#pragma once

#include <vk_mem_alloc.h>

namespace dragonfire::rendering {

class Allocation {
protected:
    static VmaAllocator allocator;
    VmaAllocation allocation{};
    VmaAllocationInfo info{};

public:
    Allocation() = default;
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
    static void destroyAllocator() noexcept {
        vmaDestroyAllocator(Allocation::allocator);
        Allocation::allocator = nullptr;
        spdlog::info("Destroyed vulkan allocator");
    }

    void* map() {
        void* ptr;
        if (vmaMapMemory(allocator, allocation, &ptr) != VK_SUCCESS)
            throw std::runtime_error("Failed to map memory");
        return ptr;
    }

    void unmap() const noexcept { vmaUnmapMemory(allocator, allocation); }

    VmaAllocationInfo& getInfo() noexcept { return info; }
};

class Buffer : public Allocation {
    vk::Buffer buffer = nullptr;

public:
    Buffer() = default;
    Buffer(const vk::BufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo);

    ~Buffer() noexcept override { reset(); };

    void reset() noexcept;

    operator vk::Buffer() noexcept { return buffer; }

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(Buffer& other) = delete;
    Buffer& operator=(Buffer& other) = delete;
};

class Image : public Allocation {
    vk::Image image;

public:
    ~Image() noexcept override { reset(); }

    void reset() noexcept;

    operator vk::Image() noexcept { return image; }

    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    Image(Image& other) = delete;
    Image& operator=(Image& other) = delete;
};

template<typename T>
class GPUObject : public Buffer {
public:
    GPUObject() = default;
    GPUObject(vk::BufferUsageFlagBits usage)
        : Buffer(
                vk::BufferCreateInfo{
                        .size = sizeof(T),
                        .usage = usage,
                        .sharingMode = vk::SharingMode::eExclusive,
                },
                VmaAllocationCreateInfo{
                        .flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT,
                        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                        .requiredFlags = (VkMemoryPropertyFlags
                        ) (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent),
                        .priority = 1,
                }
        ) {}
    T& operator*() noexcept { return *reinterpret_cast<T*>(info.pMappedData); }
    T* operator->() noexcept { return reinterpret_cast<T*>(info.pMappedData); }
};
}   // namespace dragonfire::rendering