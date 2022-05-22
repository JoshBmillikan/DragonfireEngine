//
// Created by josh on 5/21/22.
//

#include "Swapchain.h"

static vk::PresentModeKHR getPresentMode(vk::PhysicalDevice physicalDevice);
static vk::SurfaceFormatKHR getSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);

dragonfire::rendering::Swapchain::Swapchain(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        vk::SurfaceKHR surface,
        uint32_t graphicsIndex,
        uint32_t presentIndex,
        dragonfire::rendering::Swapchain* old
) {
    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    if (capabilities.currentExtent.width == 0xFFFFFFFF) {
        // TODO set extent based on swapchain
    }
    else {
        extent = capabilities.currentExtent;
    }

    uint32_t imageCount;
    if (capabilities.maxImageCount == 0) {
        imageCount = capabilities.minImageCount + 1;
    }
    else {
        imageCount = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);
    }

    auto fmt = getSurfaceFormat(physicalDevice, surface);
    vk::SharingMode sharingMode =
            graphicsIndex == presentIndex ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

    uint32_t indices[] = {graphicsIndex, presentIndex};
    vk::SwapchainCreateInfoKHR createInfo {
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

dragonfire::rendering::Swapchain::~Swapchain() {
}

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
    auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
    if (formats.empty())
        throw std::runtime_error("No surface formats available");
    for (const auto fmt : formats) {
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