//
// Created by josh on 5/21/22.
//

#pragma once

struct SDL_Window;
namespace dragonfire::rendering {
class Swapchain {
    vk::UniqueSwapchainKHR swapchain;
    vk::Extent2D extent;

public:
    Swapchain() = default;
    Swapchain(
            vk::Device device,
            vk::PhysicalDevice physicalDevice,
            vk::SurfaceKHR surface,
            uint32_t graphicsIndex,
            uint32_t presentIndex,
            SDL_Window* window,
            Swapchain* old = nullptr
    );

    Swapchain(Swapchain&& other) noexcept;
    Swapchain& operator =(Swapchain&& other) noexcept;
    Swapchain(Swapchain& other) = delete;
    Swapchain& operator =(Swapchain& other) = delete;

    ~Swapchain();
    vk::SwapchainKHR& operator *() noexcept {
        return swapchain.get();
    }
};
}   // namespace dragonfire::rendering
