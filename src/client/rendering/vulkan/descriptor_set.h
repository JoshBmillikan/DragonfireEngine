//
// Created by josh on 1/24/24.
//

#pragma once
#include "vulkan_headers.h"
#include <shared_mutex>
#include <unordered_map>

namespace dragonfire::vulkan {

class DescriptorLayoutManager {
public:
    DescriptorLayoutManager() = default;

    explicit DescriptorLayoutManager(const vk::Device device) : device(device) {}

    vk::DescriptorSetLayout createLayout(
        std::span<vk::DescriptorSetLayoutBinding> bindings,
        vk::DescriptorSetLayoutCreateFlags flags = {}
    );
    vk::DescriptorSetLayout createBindlessLayout(
        std::span<vk::DescriptorSetLayoutBinding> bindings,
        std::span<size_t> bindlessIndices,
        vk::DescriptorSetLayoutCreateFlags flags = {}
    );

    void destroy();

    ~DescriptorLayoutManager() { destroy(); }

    DescriptorLayoutManager(const DescriptorLayoutManager& other) = delete;
    DescriptorLayoutManager(DescriptorLayoutManager&& other) noexcept;
    DescriptorLayoutManager& operator=(const DescriptorLayoutManager& other) = delete;
    DescriptorLayoutManager& operator=(DescriptorLayoutManager&& other) noexcept;

private:
    std::shared_mutex mutex;
    std::unordered_map<size_t, vk::DescriptorSetLayout> layouts;
    vk::Device device;
};

}// namespace dragonfire::vulkan
