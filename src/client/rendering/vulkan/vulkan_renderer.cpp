//
// Created by josh on 1/15/24.
//

#include "vulkan_renderer.h"
#include "core/config.h"
#include <spdlog/spdlog.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace dragonfire {
vulkan::VulkanRenderer::VulkanRenderer(bool enableValidation) : BaseRenderer(SDL_WINDOW_VULKAN)
{
    enableValidation = enableValidation || std::getenv("VALIDATION_LAYERS");
    std::array enabledExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    };
    vk::PhysicalDeviceFeatures2 enabledFeatures{};
    enabledFeatures.features.samplerAnisotropy = true;
    enabledFeatures.features.sampleRateShading = true;
    enabledFeatures.features.multiDrawIndirect = true;
    vk::PhysicalDeviceVulkan12Features features12{};
    features12.descriptorBindingPartiallyBound = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.descriptorIndexing = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.drawIndirectCount = true;
    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    enabledFeatures.pNext = &features12;
    features12.pNext = &features13;

    context = Context(getWindow(), enabledExtensions, enabledFeatures, enableValidation);
    allocator = GpuAllocator(context.instance, context.physicalDevice, context.device);
    const bool vsync = Config::get().getBool("vsync").value_or(true);
    swapchain = Swapchain(getWindow(), context, vsync);
    meshRegistry = std::make_unique<MeshRegistry>(context, allocator);
}

vulkan::VulkanRenderer::~VulkanRenderer()
{
    context.device.waitIdle();
    swapchain.destroy();
    allocator.destroy();
    context.destroy();
    spdlog::get("Rendering")->info("Vulkan shutdown complete");
}
}// namespace dragonfire