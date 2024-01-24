//
// Created by josh on 1/24/24.
//

#pragma once
#include "vulkan_headers.h"
#include <unordered_map>
#include <shared_mutex>

namespace dragonfire::vulkan {

class DescriptorLayoutManager {
public:
    vk::DescriptorSetLayout createLayout(
        std::span<vk::DescriptorSetLayoutBinding> bindings,
        vk::DescriptorSetLayoutCreateFlags flags = {}
    );

private:
    std::shared_mutex mutex;
    std::unordered_map<size_t, vk::DescriptorSetLayout> layouts;
    vk::Device device;
};

}// namespace dragonfire::vulkan
