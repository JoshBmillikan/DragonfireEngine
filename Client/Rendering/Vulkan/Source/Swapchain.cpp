//
// Created by josh on 5/21/22.
//

#include "Swapchain.h"
#include <SDL_vulkan.h>
#include <alloca.h>

using namespace dragonfire::rendering;
static vk::PresentModeKHR getPresentMode(vk::PhysicalDevice physicalDevice);
static vk::SurfaceFormatKHR getSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);

Swapchain::Swapchain(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        vk::SurfaceKHR surface,
        uint32_t graphicsIndex,
        uint32_t presentIndex,
        SDL_Window* window,
        Swapchain* old
) {
    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    if (capabilities.currentExtent.width == 0xFFFFFFFF) {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        extent.width = width;
        extent.height = height;
    }
    else
        extent = capabilities.currentExtent;

    uint32_t imageCount = capabilities.maxImageCount == 0
                                  ? capabilities.minImageCount + 1
                                  : std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

    auto fmt = getSurfaceFormat(physicalDevice, surface);
    vk::SharingMode sharingMode =
            graphicsIndex == presentIndex ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

    uint32_t indices[] = {graphicsIndex, presentIndex};
    vk::SwapchainCreateInfoKHR createInfo{
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = fmt.format,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = sharingMode,
            .queueFamilyIndexCount = 2,
            .pQueueFamilyIndices = indices,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = getPresentMode(physicalDevice),
            .oldSwapchain = old->swapchain.get(),
    };

    swapchain = device.createSwapchainKHRUnique(createInfo);
    // todo
}

Swapchain::Swapchain(Swapchain&& other) noexcept {
    // todo
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
    // todo
}

Swapchain::~Swapchain() {}

static vk::PresentModeKHR getPresentMode(vk::PhysicalDevice physicalDevice) {
    auto modes = physicalDevice.getSurfacePresentModesKHR();
    for (const auto mode : modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            spdlog::info("Using mailbox presentation mode");
            return mode;
        }
    }
    spdlog::info("Using FIFO presentation mode");
    return vk::PresentModeKHR::eFifo;
}

static vk::SurfaceFormatKHR getSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) {
    uint32_t count;
    vk::SurfaceFormatKHR* formats = nullptr;
    vk::resultCheck(
            physicalDevice.getSurfaceFormatsKHR(surface, &count, formats),
            "Failed to get surface format count"
    );
    if (count == 0)
        throw std::runtime_error("No surface formats available");
    formats = (vk::SurfaceFormatKHR*) alloca(sizeof(vk::SurfaceFormatKHR) * count);
    vk::resultCheck(physicalDevice.getSurfaceFormatsKHR(surface, &count, formats), "Failed to get surface formats");

    for (uint32_t i = 0; i < count; i++) {
        auto& fmt = formats[i];
        if (fmt.format == vk::Format::eB8G8R8Srgb && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return fmt;
    }
    spdlog::warn(
            "Preferred surface format not found, using format: {}, color space: {} instead",
            to_string(formats[0].format),
            to_string(formats[0].colorSpace)
    );
    return formats[0];
}