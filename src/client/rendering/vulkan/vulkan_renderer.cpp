//
// Created by josh on 1/15/24.
//

#include "vulkan_renderer.h"
#include "core/config.h"
#include "core/crash.h"
#include "core/utility/utility.h"
#include "gltf_loader.h"
#include "vulkan_material.h"

#include <core/utility/math_utils.h>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_hash.hpp>
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
    textureRegistry = std::make_unique<TextureRegistry>(allocator);

    initBuffers();

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        frames[i] = Frame(context, allocator, maxDrawCount);
    presentThread = std::jthread(std::bind_front(&VulkanRenderer::present, this));
}

void vulkan::VulkanRenderer::beginFrame(const Camera& camera)
{
    waitForLastFrame();
    writeGlobalUBO(camera);
    const Frame& frame = getCurrentFrame();
    context.device.resetCommandPool(frame.pool);
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    frame.cmd.begin(beginInfo);
}

void vulkan::VulkanRenderer::drawModels(const Camera& camera, const Drawables& models)
{
    if (models.empty())
        return;
    uint32_t drawCount = 0, pipelineCount = 0;
    const Frame& frame = getCurrentFrame();
    DrawData* drawData = static_cast<DrawData*>(frame.drawData.getInfo().pMappedData);
    for (auto& [material, draws] : models) {
        const auto mat
            = static_cast<const VulkanMaterial*>(material);// NOLINT(*-pro-type-static-cast-downcast)
        for (auto& draw : draws) {
            if (drawCount >= maxDrawCount) {
                logger->error("Max draw count exceeded, some models may not be drawn");
                break;
            }
            const Pipeline& pipeline = mat->pipeline;
            if (pipelineMap.contains(pipeline))
                pipelineMap[pipeline].drawCount++;
            else {
                pipelineMap[pipeline] = {
                    .index = pipelineCount++,
                    .drawCount = 1,
                    .layout = pipeline.getLayout(),
                };
            }
            DrawData& data = drawData[drawCount++];
            data.transform = draw.transform;
            data.boundingSphere = draw.bounds;
            data.boundingSphere.w *= getMatrixScaleFactor(draw.transform);
            const Mesh* mesh = reinterpret_cast<Mesh*>(draw.mesh);
            data.vertexOffset = mesh->vertexInfo.offset;
            data.indexOffset = mesh->indexInfo.offset;
            data.vertexCount = mesh->vertexCount;
            data.indexCount = mesh->indexCount;
            data.textureIndices = mat->getTextures();
        }
    }
    computePrePass(drawCount, true);
    beginRendering();
    mainPass();
    frame.cmd.endRendering();
}

void vulkan::VulkanRenderer::endFrame()
{
    transistionImageLayout(
        swapchain.currentImage(),
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eNone,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe
    );
    Frame& frame = getCurrentFrame();
    frame.cmd.end();
    {
        std::unique_lock lock(presentData.mutex);
        presentData.frame = &frame;
    }
    presentData.condVar.notify_one();
    pipelineMap.clear();
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
    textureRegistry.reset();
    msaaImage.destroy();
    depthBuffer.destroy();
    context.device.destroy(depthView);
    context.device.destroy(msaaView);
    globalUBO.destroy();
    swapchain.destroy();
    allocator.destroy();
    context.destroy();
    spdlog::get("Rendering")->info("Vulkan shutdown complete");
}

std::unique_ptr<Model::Loader> vulkan::VulkanRenderer::getModelLoader()
{
    return std::make_unique<VulkanGltfLoader>(
        context,
        *meshRegistry,
        *textureRegistry,
        allocator,
        pipelineFactory.get()
    );
}

vulkan::VulkanRenderer::Frame::Frame(
    const Context& ctx,
    const GpuAllocator& allocator,
    const uint32_t maxDrawCount
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

void vulkan::VulkanRenderer::computePrePass(const uint32_t drawCount, const bool cull)
{
    const Frame& frame = getCurrentFrame();
    const vk::CommandBuffer cmd = frame.cmd;
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, cullComputeLayout, 0, frame.computeSet, {});
    cullPipeline.bind(cmd);
    uint32_t baseIndex = 0;
    for (const auto& info : std::ranges::views::values(pipelineMap)) {
        if (info.drawCount == 0)
            continue;
        const uint32_t pushConstants[] = {baseIndex, info.index, drawCount, cull ? 1u : 0};
        cmd.pushConstants(
            cullComputeLayout,
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(uint32_t) * 4,
            pushConstants
        );
        cmd.dispatch(std::max(drawCount / 256u, 1u), 1, 1);
        baseIndex += info.drawCount;
    }
    vk::BufferMemoryBarrier commands{}, count{};
    commands.buffer = frame.commandBuffer;
    commands.size = frame.commandBuffer.getInfo().size;
    count.buffer = frame.countBuffer;
    count.size = frame.countBuffer.getInfo().size;
    commands.offset = count.offset = 0;
    commands.srcAccessMask = count.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    commands.dstAccessMask = count.dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eDrawIndirect,
        {},
        {},
        {commands, count},
        {}
    );
}

void vulkan::VulkanRenderer::mainPass()
{
    const Frame& frame = getCurrentFrame();
    const vk::CommandBuffer cmd = frame.cmd;
    vk::Viewport viewport{};
    viewport.x = viewport.y = 0.0f;
    viewport.width = float(swapchain.getExtent().width);
    viewport.height = float(swapchain.getExtent().height);
    viewport.maxDepth = 1.0f;
    viewport.minDepth = 0.0f;
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{};
    scissor.extent = swapchain.getExtent();
    cmd.setScissor(0, scissor);

    meshRegistry->bindBuffers(cmd);
    vk::DeviceSize drawOffset = 0;
    for (auto& [pipeline, info] : pipelineMap) {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            info.layout,
            0,
            {frame.globalDescriptorSet, frame.frameSet},
            {}
        );
        cmd.drawIndexedIndirectCount(
            frame.commandBuffer,
            drawOffset,
            frame.countBuffer,
            info.index * sizeof(uint32_t),
            maxDrawCount,
            sizeof(vk::DrawIndexedIndirectCommand)
        );
        drawOffset += info.drawCount * sizeof(vk::DrawIndexedIndirectCommand);
    }
}

void vulkan::VulkanRenderer::beginRendering()
{
    transistionImageLayout(
        swapchain.currentImage(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    vk::RenderingAttachmentInfo color{}, depth{};
    color.clearValue = vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f};
    color.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color.imageView = swapchain.currentView();
    color.loadOp = vk::AttachmentLoadOp::eClear;
    color.storeOp = vk::AttachmentStoreOp::eStore;
    color.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color.resolveImageView = msaaView;
    color.resolveMode = vk::ResolveModeFlagBits::eAverage;

    depth.clearValue = vk::ClearDepthStencilValue{1.0f, 0};
    depth.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depth.imageView = depthView;
    depth.loadOp = vk::AttachmentLoadOp::eClear;
    depth.storeOp = vk::AttachmentStoreOp::eDontCare;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &color;
    renderingInfo.pDepthAttachment = &depth;
    renderingInfo.layerCount = 1;
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D{}, swapchain.getExtent());

    getCurrentFrame().cmd.beginRendering(renderingInfo);
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
    logger->trace("Presentation thread destroyed");
}

void vulkan::VulkanRenderer::transistionImageLayout(
    const vk::Image image,
    const vk::ImageLayout from,
    const vk::ImageLayout to,
    const vk::AccessFlags srcStage,
    const vk::AccessFlags dstStage,
    const vk::PipelineStageFlags pipelineStart,
    const vk::PipelineStageFlags pipelineEnd,
    const vk::ImageAspectFlags imageAspect,
    const uint32_t baseMipLevel,
    const uint32_t levelCount,
    const uint32_t baseArrayLayer,
    const uint32_t layerCount
)
{
    vk::ImageMemoryBarrier barrier{};
    barrier.image = image;
    barrier.srcAccessMask = srcStage;
    barrier.dstAccessMask = dstStage;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.subresourceRange.aspectMask = imageAspect;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;

    const vk::CommandBuffer cmd = getCurrentFrame().cmd;
    cmd.pipelineBarrier(pipelineStart, pipelineEnd, {}, {}, {}, barrier);
}

static vk::Format getDepthFormat(const vk::PhysicalDevice physicalDevice)
{
    constexpr std::array possibleFormats = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
    };
    for (const vk::Format fmt : possibleFormats) {
        auto props = physicalDevice.getFormatProperties(fmt);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            return fmt;
    }
    crash("No valid depth image format found");
}

void vulkan::VulkanRenderer::initBuffers()
{
    vk::BufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.size = padToAlignment(sizeof(UBOData), 16) * 2;
    bufferCreateInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
    bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    allocInfo.priority = 1.0f;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    globalUBO = allocator.allocate(bufferCreateInfo, allocInfo, "global UBO");

    const vk::Format depthFormat = getDepthFormat(context.physicalDevice);
    vk::ImageCreateInfo imageCreateInfo{};
    imageCreateInfo.extent = vk::Extent3D(swapchain.getExtent(), 1);
    imageCreateInfo.format = depthFormat;
    imageCreateInfo.samples = context.sampleCount;
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    allocInfo.preferredFlags = 0;
    allocInfo.flags = 0;
    depthBuffer = allocator.allocate(imageCreateInfo, allocInfo, "depth buffer");

    imageCreateInfo.format = swapchain.getFormat();
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransientAttachment
                            | vk::ImageUsageFlagBits::eColorAttachment;
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    msaaImage = allocator.allocate(imageCreateInfo, allocInfo);

    vk::ImageSubresourceRange subRange{};
    subRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    subRange.layerCount = 1;
    subRange.levelCount = 1;
    subRange.baseArrayLayer = 0;
    subRange.baseMipLevel = 0;
    depthView = depthBuffer.createView(context.device, subRange);
    subRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    try {
        msaaView = msaaImage.createView(context.device, subRange);
    }
    catch (...) {
        context.device.destroy(depthView);
        throw;
    }
}

}// namespace dragonfire