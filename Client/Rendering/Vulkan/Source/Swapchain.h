//
// Created by josh on 5/21/22.
//

#pragma once

namespace dragonfire::rendering {
class Swapchain {
    vk::UniqueSwapchainKHR swapchain;
    vk::Extent2D extent;

public:
    Swapchain(
            vk::Device device,
            vk::PhysicalDevice physicalDevice,
            vk::SurfaceKHR surface,
            uint32_t graphicsIndex,
            uint32_t presentIndex,
            Swapchain* old = nullptr
    );
    ~Swapchain();
    vk::SwapchainKHR& operator *() noexcept {
        return swapchain.get();
    }
};
}   // namespace dragonfire::rendering
