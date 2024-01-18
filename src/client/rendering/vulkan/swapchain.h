//
// Created by josh on 1/17/24.
//

#pragma once
#include "vulkan_headers.h"
#include <SDL_video.h>

namespace dragonfire::vulkan {

class Swapchain {
public:
    Swapchain() = default;
    Swapchain(SDL_Window* window, const struct Context& ctx, bool vsync, vk::SwapchainKHR old = nullptr);

    operator vk::SwapchainKHR() const { return swapchain; }

    void destroy();
    vk::Result next(vk::Semaphore semaphore = nullptr, vk::Fence fence = nullptr) noexcept;

    ~Swapchain() { destroy(); }

    Swapchain(Swapchain&) = delete;
    Swapchain& operator=(Swapchain&) = delete;
    Swapchain(Swapchain&& other) noexcept;
    Swapchain& operator=(Swapchain&& other) noexcept;

    [[nodiscard]] uint32_t getCurrentImageIndex() const { return currentImageIndex; }

    [[nodiscard]] uint32_t getImageCount() const { return imageCount; }

    [[nodiscard]] vk::Image currentImage() const
    {
        return images ? images[currentImageIndex % imageCount] : nullptr;
    }

    [[nodiscard]] vk::ImageView currentView() const
    {
        return views ? views[currentImageIndex % imageCount] : nullptr;
    }

    [[nodiscard]] vk::Extent2D getExtent() const { return extent; }

    [[nodiscard]] vk::Format getFormat() const { return format; }

private:
    vk::SwapchainKHR swapchain;
    vk::Format format = vk::Format::eUndefined;
    vk::Extent2D extent{};
    uint32_t imageCount = 0, currentImageIndex = 0;
    vk::Image* images = nullptr;
    vk::ImageView* views = nullptr;
    vk::Device device;
};

}// namespace dragonfire::vulkan
