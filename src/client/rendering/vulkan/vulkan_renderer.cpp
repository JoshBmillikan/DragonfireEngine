//
// Created by josh on 1/15/24.
//

#include "vulkan_renderer.h"
#include "core/config.h"
#include "core/crash.h"
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_to_string.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace dragonfire {

vulkan::VulkanRenderer::VulkanRenderer(bool enableValidation)
    : BaseRenderer(SDL_WINDOW_VULKAN), maxDrawCount(Config::get().getInt("maxDrawCount").value_or(1 << 14))
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
        frames[i] = Frame(context, allocator, maxDrawCount);
    presentThread = std::jthread(std::bind_front(&VulkanRenderer::present, this));
}

void vulkan::VulkanRenderer::beginFrame(const Camera& camera)
{
    waitForLastFrame();
    writeGlobalUBO(camera);
}

void vulkan::VulkanRenderer::drawModels(const Camera& camera, const Drawables& models) {}

void vulkan::VulkanRenderer::endFrame()
{
    Frame& frame = getCurrentFrame();
    frame.cmd.end();
    {
        std::unique_lock lock(presentData.mutex);
        presentData.frame = &frame;
    }
    presentData.condVar.notify_one();
    BaseRenderer::endFrame();
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

vulkan::VulkanRenderer::Frame::Frame(
    const Context& ctx,
    const GpuAllocator& allocator,
    const size_t maxDrawCount
)
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
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    bufferInfo.size = maxDrawCount * sizeof(DrawData);
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    drawData = allocator.allocate(bufferInfo, allocInfo);
    bufferInfo.size = maxDrawCount * sizeof(glm::mat4);
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    allocInfo.flags = 0;
    culledMatrices = allocator.allocate(bufferInfo, allocInfo);
    bufferInfo.usage |= vk::BufferUsageFlagBits::eIndirectBuffer;
    bufferInfo.size = maxDrawCount * sizeof(vk::DrawIndexedIndirectCommand);
    commandBuffer = allocator.allocate(bufferInfo, allocInfo);
    bufferInfo.size = maxDrawCount * sizeof(uint32_t);
    allocInfo.preferredFlags = 0;
    countBuffer = allocator.allocate(bufferInfo, allocInfo);
    bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    bufferInfo.size = maxDrawCount * sizeof(TextureIds);
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    textureIndexBuffer = allocator.allocate(bufferInfo, allocInfo);

    // TODO descriptor sets
}

void vulkan::VulkanRenderer::waitForLastFrame()
{
    vk::Result result;
    {
        std::unique_lock lock(presentData.mutex);
        if (presentData.frame)
            presentData.condVar.wait(lock, [&] { return presentData.frame == nullptr; });
        result = presentData.result;
    }
    const Frame& frame = getCurrentFrame();
    if (context.device.waitForFences(frame.fence, true, UINT64_MAX) != vk::Result::eSuccess)
        logger->error("Fence wait failed, attempting to continue, but things may break");

    int retries = 0;
    do {
        if (retries > 10)
            crash("Swapchain recreation failed, maximum number of retries exceeded");
        switch (result) {
            case vk::Result::eSuccess: break;
            case vk::Result::eSuboptimalKHR:
            case vk::Result::eErrorOutOfDateKHR: {
                const bool vsync = Config::get().getBool("vsync").value_or(true);
                context.device.waitIdle();
                swapchain = Swapchain(getWindow(), context, vsync, swapchain);
                break;
            }
            default: crash("Failed to acquire next swapchain image: {}", to_string(result));
        }
        result = swapchain.next(frame.renderingSemaphore);
        retries++;
    } while (result != vk::Result::eSuccess);
    context.device.resetFences(frame.fence);
}

void vulkan::VulkanRenderer::writeGlobalUBO(const Camera& camera) const
{
    auto ptr = static_cast<char*>(globalUBO.getInfo().pMappedData);
    ptr += uboOffset * (getFrameCount() % FRAMES_IN_FLIGHT);
    const auto data = reinterpret_cast<UBOData*>(ptr);
    const glm::mat4 view = camera.getViewMatrix();
    data->orthographic = camera.orthograhpic * view;
    data->perspective = camera.perspective * view;
    data->view = view;
    data->resolution = glm::vec2(swapchain.getExtent().width, swapchain.getExtent().height);
    data->cameraPosition = glm::vec3(view * glm::vec4(camera.position, 1.0));
    data->sunDirection = glm::normalize(glm::vec3(-0.2f, -0.3f, 1.0f));
    auto [frustumX, frustumY] = camera.getFrustumPlanes();
    data->frustum.x = frustumX.x;
    data->frustum.y = frustumX.z;
    data->frustum.z = frustumY.y;
    data->frustum.w = frustumY.z;
    data->P00 = camera.perspective[0][0];
    data->P11 = camera.perspective[1][1];
    data->zNear = camera.getZNear();
    data->zFar = camera.getZFar();
}

void vulkan::VulkanRenderer::present(const std::stop_token& token)
{
    while (!token.stop_requested()) {
        std::unique_lock lock(presentData.mutex);
        presentData.condVar.wait(lock, token, [&] { return presentData.frame != nullptr; });
        if (token.stop_requested() || presentData.frame == nullptr)
            break;

        vk::CommandBufferSubmitInfo cmdInfo{};
        cmdInfo.commandBuffer = presentData.frame->cmd;
        vk::SemaphoreSubmitInfo signal{}, wait{};
        signal.semaphore = presentData.frame->presentSemaphore;
        wait.semaphore = presentData.frame->renderingSemaphore;
        signal.stageMask = vk::PipelineStageFlagBits2::eAllGraphics;
        wait.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;

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
        presentData.frame = nullptr;
        lock.unlock();
        presentData.condVar.notify_one();
    }
    context.queues.present.waitIdle();
    logger->trace("Presentation thread destroyed");
}

}// namespace dragonfire