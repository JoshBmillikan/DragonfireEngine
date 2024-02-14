//
// Created by josh on 1/15/24.
//

#include "vulkan_renderer.h"
#include "core/config.h"
#include "core/crash.h"
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
    descriptorLayoutManager = DescriptorLayoutManager(context.device);
    pipelineFactory = std::make_unique<PipelineFactory>(context, &descriptorLayoutManager);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        frames[i] = Frame(context, allocator);
    presentThread = std::jthread(std::bind_front(&VulkanRenderer::present, this));
}

vulkan::VulkanRenderer::~VulkanRenderer()
{
    presentThread.request_stop();
    context.device.waitIdle();
    presentThread.join();
    for (Frame& frame : frames) {
        context.device.destroy(frame.pool);
        context.device.destroy(frame.presentSemaphore);
        context.device.destroy(frame.renderingSemaphore);
        context.device.destroy(frame.fence);
        frame.drawData.destroy();
        frame.culledMatrices.destroy();
        frame.commandBuffer.destroy();
        frame.countBuffer.destroy();
        frame.textureIndexBuffer.destroy();
    }
    pipelineFactory.reset();
    descriptorLayoutManager.destroy();
    meshRegistry.reset();
    swapchain.destroy();
    allocator.destroy();
    context.destroy();
    spdlog::get("Rendering")->info("Vulkan shutdown complete");
}

vulkan::VulkanRenderer::Frame::Frame(const Context& ctx, const GpuAllocator& allocator)
{
    vk::CommandPoolCreateInfo createInfo{};
    createInfo.queueFamilyIndex = ctx.queues.graphicsFamily;
    createInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
    pool = ctx.device.createCommandPool(createInfo);
    vk::CommandBufferAllocateInfo cmdInfo{};
    cmdInfo.commandPool = pool;
    cmdInfo.level = vk::CommandBufferLevel::ePrimary;
    cmdInfo.commandBufferCount = 1;
    if (ctx.device.allocateCommandBuffers(&cmdInfo, &cmd) != vk::Result::eSuccess)
        crash("Failed to allocate command bufferes");
    fence = ctx.device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
    renderingSemaphore = ctx.device.createSemaphore(vk::SemaphoreCreateInfo());
    presentSemaphore = ctx.device.createSemaphore(vk::SemaphoreCreateInfo());

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    //TODO
    VmaAllocationCreateInfo allocInfo{};
}

void vulkan::VulkanRenderer::present(const std::stop_token& token)
{
    while (!token.stop_requested()) {
        std::unique_lock lock(presentData.mutex);
        presentData.condVar.wait(lock, token, [&] { return presentData.frame != nullptr; });
        if (token.stop_requested())
            break;

        vk::CommandBufferSubmitInfo cmdInfo{};
        cmdInfo.commandBuffer = presentData.frame->cmd;
        vk::SemaphoreSubmitInfo signal{}, wait{};
        signal.semaphore = presentData.frame->presentSemaphore;
        wait.semaphore = presentData.frame->renderingSemaphore;
        // todo semaphore
        vk::SubmitInfo2 submitInfo{};
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &cmdInfo;
        submitInfo.signalSemaphoreInfoCount = submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signal;
        submitInfo.pWaitSemaphoreInfos = &wait;

        context.queues.graphics.submit2(submitInfo);

        vk::PresentInfoKHR presentInfo{};
        vk::SwapchainKHR swapchainKhr = swapchain;
        presentInfo.pSwapchains = &swapchainKhr;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &presentData.imageIndex;
        presentInfo.pWaitSemaphores = &presentData.frame->presentSemaphore;
        presentInfo.waitSemaphoreCount = 1;
        presentData.result = context.queues.present.presentKHR(presentInfo);
    }
}

}// namespace dragonfire