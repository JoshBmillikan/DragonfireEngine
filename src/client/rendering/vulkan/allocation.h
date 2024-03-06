//
// Created by josh on 1/17/24.
//

#pragma once
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vulkan_headers.h"
#include <vk_mem_alloc.h>

namespace dragonfire::vulkan {

class GpuAllocation {
public:
    virtual ~GpuAllocation() = default;

    [[nodiscard]] const VmaAllocationInfo& getInfo() const { return info; }

    [[nodiscard]] void* map() const;
    void unmap() const;
    void flush() const;

    GpuAllocation(const GpuAllocation& other) = delete;
    GpuAllocation(GpuAllocation&& other) noexcept;
    GpuAllocation& operator=(const GpuAllocation& other) = delete;
    GpuAllocation& operator=(GpuAllocation&& other) noexcept;
    void setName(const char* name) const;

protected:
    GpuAllocation() = default;
    VmaAllocation allocation = nullptr;
    VmaAllocator allocator = nullptr;
    VmaAllocationInfo info{};
};

class Buffer final : public GpuAllocation {
    friend class GpuAllocator;
    vk::Buffer buffer;

public:
    Buffer() = default;

    operator vk::Buffer() const { return buffer; }

    ~Buffer() noexcept override { destroy(); }

    void destroy() noexcept;

    Buffer(const Buffer& other) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(const Buffer& other) = delete;
    Buffer& operator=(Buffer&& other) noexcept;
};

class Image final : public GpuAllocation {
    friend class GpuAllocator;
    vk::Image image;
    vk::Format format{};
    vk::Extent3D extent{};

public:
    Image() = default;

    [[nodiscard]] vk::Format getFormat() const { return format; }

    ~Image() override { destroy(); }

    void destroy() noexcept;
    Image(const Image& other) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(const Image& other) = delete;
    Image& operator=(Image&& other) noexcept;

    [[nodiscard]] vk::ImageView createView(
        vk::Device device,
        const vk::ImageSubresourceRange& subresourceRange,
        vk::ImageViewType type = vk::ImageViewType::e2D
    ) const;
};

class GpuAllocator {
    VmaAllocator allocator = nullptr;

public:
    GpuAllocator() = default;
    GpuAllocator(
        vk::Instance instance,
        vk::PhysicalDevice physicalDevice,
        vk::Device device,
        const VULKAN_HPP_DEFAULT_DISPATCHER_TYPE& ld = VULKAN_HPP_DEFAULT_DISPATCHER
    ) noexcept;

    explicit GpuAllocator(VmaAllocator allocator) : allocator(allocator) {}

    GpuAllocator(const GpuAllocator& other) = default;

    GpuAllocator(GpuAllocator&& other) noexcept : allocator(other.allocator) { other.allocator = nullptr; };

    GpuAllocator& operator=(const GpuAllocator& other) = default;

    GpuAllocator& operator=(GpuAllocator&& other) noexcept
    {
        if (this != &other) {
            allocator = other.allocator;
            other.allocator = nullptr;
        }
        return *this;
    }

    [[nodiscard]] VmaAllocator get() const { return allocator; }

    [[nodiscard]] Buffer allocate(
        const vk::BufferCreateInfo& createInfo,
        const VmaAllocationCreateInfo& allocInfo,
        const char* allocationName = nullptr
    ) const;
    [[nodiscard]] Image allocate(
        const vk::ImageCreateInfo& createInfo,
        const VmaAllocationCreateInfo& allocInfo,
        const char* allocationName = nullptr
    ) const;

    void destroy();
    ~GpuAllocator();
};

}// namespace dragonfire::vulkan
