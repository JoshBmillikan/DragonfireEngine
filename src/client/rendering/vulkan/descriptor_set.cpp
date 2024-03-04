//
// Created by josh on 1/24/24.
//

#include "descriptor_set.h"
#include <core/utility/utility.h>
#include <mutex>
#include <ranges>
#include <vulkan/vulkan_hash.hpp>

namespace dragonfire::vulkan {
vk::DescriptorSetLayout DescriptorLayoutManager::createLayout(
    const std::span<vk::DescriptorSetLayoutBinding> bindings,
    const vk::DescriptorSetLayoutCreateFlags flags
)
{
    size_t hash = std::hash<vk::DescriptorSetLayoutCreateFlags>()(flags);
    for (auto& binding : bindings)
        hashCombine(hash, binding);

    {
        std::shared_lock lock(mutex);
        const auto iter = layouts.find(hash);
        if (iter != layouts.end())
            return iter->second;
    }

    vk::DescriptorSetLayoutCreateInfo createInfo{};
    createInfo.flags = flags;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings = bindings.data();
    const vk::DescriptorSetLayout layout = device.createDescriptorSetLayout(createInfo);
    std::unique_lock lock(mutex);
    layouts[hash] = layout;
    return layout;
}

void DescriptorLayoutManager::destroy()
{
    if (device) {
        for (const auto layout : std::ranges::views::values(layouts))
            device.destroy(layout);
        layouts.clear();
        device = nullptr;
    }
}

DescriptorLayoutManager::DescriptorLayoutManager(DescriptorLayoutManager&& other) noexcept
{
    std::scoped_lock lock(other.mutex);
    device = other.device;
    other.device = nullptr;
    layouts = std::move(other.layouts);
}

DescriptorLayoutManager& DescriptorLayoutManager::operator=(DescriptorLayoutManager&& other) noexcept
{
    if (this != &other) {
        std::scoped_lock lock(mutex, other.mutex);
        device = other.device;
        other.device = nullptr;
        layouts = std::move(layouts);
    }
    return *this;
}
}// namespace dragonfire::vulkan
