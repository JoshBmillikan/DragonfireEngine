//
// Created by josh on 1/17/24.
//

#include "swapchain.h"
#include "context.h"
#include <SDL2/SDL_vulkan.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_to_string.hpp>

namespace dragonfire::vulkan {

static vk::PresentModeKHR getPresentMode(
    const bool vsync,
    const Context& ctx,
    spdlog::logger* logger
) noexcept
{
    const vk::PresentModeKHR target = vsync ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eImmediate;
    uint32_t modeCount;
    vk::PresentModeKHR* modes = nullptr;
    vk::Result result = ctx.physicalDevice.getSurfacePresentModesKHR(ctx.surface, &modeCount, modes);
    if (result == vk::Result::eSuccess) {
        modes = static_cast<vk::PresentModeKHR*>(alloca(sizeof(vk::PresentModeKHR) * modeCount));
        result = ctx.physicalDevice.getSurfacePresentModesKHR(ctx.surface, &modeCount, modes);
    }
    if (result != vk::Result::eSuccess) {
        logger->error("Could not get surface presentation modes");
        return vk::PresentModeKHR::eFifo;
    }
    for (uint32_t i = 0; i < modeCount; i++) {
        if (modes && modes[i] == target)
            return target;
    }
    SPDLOG_LOGGER_WARN(
        logger,
        "Preferred presentation mode {} not available, falling back to FIFO",
        to_string(target)
    );
    return vk::PresentModeKHR::eFifo;
}

static vk::SurfaceFormatKHR getSurfaceFormat(const Context& ctx, spdlog::logger* logger)
{
    uint32_t formatCount;
    vk::SurfaceFormatKHR* formats = nullptr;
    vk::Result result = ctx.physicalDevice.getSurfaceFormatsKHR(ctx.surface, &formatCount, formats);
    if (result == vk::Result::eSuccess) {
        formats = static_cast<vk::SurfaceFormatKHR*>(alloca(sizeof(vk::SurfaceFormatKHR) * formatCount));
        result = ctx.physicalDevice.getSurfaceFormatsKHR(ctx.surface, &formatCount, formats);
    }
    if (result != vk::Result::eSuccess || formatCount == 0 || formats == nullptr)
        throw std::runtime_error("Failed to get surface formats");
    for (uint32_t i = 0; i < formatCount; i++) {
        const vk::SurfaceFormatKHR fmt = formats[i];
        if (fmt.format == vk::Format::eB8G8R8A8Srgb && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return fmt;
    }

    for (uint32_t i = 0; i < formatCount; i++) {
        const vk::SurfaceFormatKHR fmt = formats[i];
        if (fmt.format == vk::Format::eR8G8B8A8Srgb && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return fmt;
    }

    for (uint32_t i = 0; i < formatCount; i++) {
        const vk::SurfaceFormatKHR fmt = formats[i];
        if (fmt.format == vk::Format::eB8G8R8A8Unorm && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return fmt;
    }

    SPDLOG_LOGGER_WARN(
        logger,
        "Preferred surface format not found, falling back to first supported format({})",
        to_string(formats[0].format)
    );
    return formats[0];
}

static vk::ImageView* createImageViews(
    const vk::Image* images,
    const uint32_t imageCount,
    const vk::Format format,
    const vk::Device device
) noexcept
{
    const auto views = new (std::nothrow) vk::ImageView[imageCount];
    if (views == nullptr)
        return nullptr;
    for (uint32_t i = 0; i < imageCount; i++) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.format = format;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.image = images[i];
        viewInfo.components.r = vk::ComponentSwizzle::eIdentity;
        viewInfo.components.g = vk::ComponentSwizzle::eIdentity;
        viewInfo.components.b = vk::ComponentSwizzle::eIdentity;
        viewInfo.components.a = vk::ComponentSwizzle::eIdentity;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (device.createImageView(&viewInfo, nullptr, views + i) != vk::Result::eSuccess) {
            for (uint32_t j = 0; j < i; j++)
                device.destroy(views[j]);
            delete[] views;
            return nullptr;
        }
    }
    return views;
}

Swapchain::Swapchain(SDL_Window* window, const Context& ctx, const bool vsync, const vk::SwapchainKHR old)
    : device(ctx.device)
{
    const auto capabilities = ctx.physicalDevice.getSurfaceCapabilitiesKHR(ctx.surface);
    extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX || extent.height == UINT32_MAX) {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        extent.width = width;
        extent.height = height;
    }

    uint32_t minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0)
        minImageCount = std::min(capabilities.maxImageCount, minImageCount);
    const vk::SharingMode shareMode = ctx.queues.graphicsFamily == ctx.queues.presentFamily
                                          ? vk::SharingMode::eExclusive
                                          : vk::SharingMode::eConcurrent;
    const auto logger = spdlog::get("Rendering");
    const vk::PresentModeKHR presentMode = getPresentMode(vsync, ctx, logger.get());
    const vk::SurfaceFormatKHR surfaceFormat = getSurfaceFormat(ctx, logger.get());
    logger->info("Using swapchain surface format {}", to_string(surfaceFormat.format));
    format = surfaceFormat.format;

    const uint32_t queueIndices[] = {ctx.queues.graphicsFamily, ctx.queues.presentFamily};
    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = ctx.surface;
    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = format;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    createInfo.imageSharingMode = shareMode;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueIndices;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.oldSwapchain = old;
    swapchain = ctx.device.createSwapchainKHR(createInfo);

    vk::Result result = ctx.device.getSwapchainImagesKHR(swapchain, &imageCount, nullptr);
    if (result == vk::Result::eSuccess) {
        images = new (std::nothrow) vk::Image[imageCount];
        result = ctx.device.getSwapchainImagesKHR(swapchain, &imageCount, images);
    }
    if (images == nullptr || result != vk::Result::eSuccess) {
        ctx.device.destroy(swapchain);
        delete[] images;
        throw std::runtime_error("Failed to get swapchain images");
    }
    views = createImageViews(images, imageCount, format, ctx.device);
    if (views == nullptr) {
        ctx.device.destroy(swapchain);
        delete[] images;
        throw std::runtime_error("Failed to create swapchain image views");
    }
    logger->info("Created swapchain of extent {}x{} with {} images", extent.width, extent.height, imageCount);
}

vk::Result Swapchain::next(const vk::Semaphore semaphore, const vk::Fence fence) noexcept
{
    auto [result, index] = device.acquireNextImageKHR(swapchain, UINT64_MAX, semaphore, fence);
    if (result == vk::Result::eSuccess)
        currentImageIndex = index;
    return result;
}

Swapchain::Swapchain(Swapchain&& other) noexcept
    : swapchain(other.swapchain), format(other.format), extent(other.extent), imageCount(other.imageCount),
      currentImageIndex(other.currentImageIndex), images(other.images), views(other.views),
      device(other.device)
{
    other.swapchain = nullptr;
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept
{
    if (this == &other)
        return *this;
    swapchain = other.swapchain;
    other.swapchain = nullptr;
    format = other.format;
    extent = other.extent;
    imageCount = other.imageCount;
    currentImageIndex = other.currentImageIndex;
    images = other.images;
    views = other.views;
    device = other.device;
    return *this;
}

void Swapchain::destroy()
{
    if (swapchain) {
        for (uint32_t i = 0; i < imageCount; i++) {
            if (views)
                device.destroy(views[i]);
        }
        device.destroy(swapchain);
        swapchain = nullptr;
        delete[] images;
        images = nullptr;
        delete[] views;
        views = nullptr;
        SPDLOG_LOGGER_DEBUG(spdlog::get("Rendering"), "Swapchain destroyed");
    }
}
}// namespace dragonfire::vulkan
