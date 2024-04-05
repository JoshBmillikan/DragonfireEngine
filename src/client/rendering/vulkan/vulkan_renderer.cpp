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
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_hash.hpp>
#include <vulkan/vulkan_to_string.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace dragonfire {

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

vulkan::VulkanRenderer::VulkanRenderer(bool enableValidation)
    : BaseRenderer(SDL_WINDOW_VULKAN, ImGui_ImplVulkan_NewFrame),
      maxDrawCount(Config::get().getInt("maxDrawCount").value_or(1 << 14))
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
    pipelineFactory = std::make_unique<PipelineFactory>(
        context,
        &descriptorLayoutManager,
        getDepthFormat(context.physicalDevice),
        swapchain.getFormat(),
        maxDrawCount
    );
    textureRegistry = std::make_unique<TextureRegistry>(context, allocator);

    vk::DescriptorPoolSize sizes[]
        = {{vk::DescriptorType::eUniformBuffer, 16},
           {vk::DescriptorType::eCombinedImageSampler, maxDrawCount},
           {vk::DescriptorType::eStorageBuffer, 64}};
    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind
                                     | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptorPoolCreateInfo.poolSizeCount = 3;
    descriptorPoolCreateInfo.pPoolSizes = sizes;
    descriptorPoolCreateInfo.maxSets = FRAMES_IN_FLIGHT * 16 + maxDrawCount;
    descriptorPool = context.device.createDescriptorPool(descriptorPoolCreateInfo);

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
    initImages();
    cullPipeline = createComputePipeline();

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        frames[i] = Frame(
            context,
            descriptorPool,
            allocator,
            maxDrawCount,
            i,
            globalUBO,
            uboOffset,
            descriptorLayoutManager
        );
    presentThread = std::jthread(std::bind_front(&VulkanRenderer::present, this));
    initImGui();
}

void vulkan::VulkanRenderer::setVsync(const bool vsync)
{
    BaseRenderer::setVsync(vsync);
    recreateSwapchain(vsync);
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

void vulkan::VulkanRenderer::drawModels(const Camera& camera, const Drawable::Drawables& models)
{
    if (models.empty())
        return;
    uint32_t drawCount = 0, pipelineCount = 0;
    const Frame& frame = getCurrentFrame();
    DrawData* drawData = static_cast<DrawData*>(frame.drawData.getInfo().pMappedData);
    for (auto& [material, draws] : models) {
        const auto mat = dynamic_cast<const VulkanMaterial*>(material);
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
            data.vertexOffset = mesh->vertexInfo.offset / sizeof(Vertex);
            data.indexOffset = mesh->indexInfo.offset / sizeof(uint32_t);
            data.vertexCount = mesh->vertexCount;
            data.indexCount = mesh->indexCount;
            data.textureIndices = mat->getTextures();
        }
    }
    computePrePass(drawCount, true);
    beginRendering();
    mainPass();
    auto* d = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(d, getCurrentFrame().cmd);
    frame.cmd.endRendering();
}

void vulkan::VulkanRenderer::endFrame()
{
    transitionImageLayout(
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
        presentData.imageIndex = swapchain.getCurrentImageIndex();
    }
    presentData.condVar.notify_one();
    pipelineMap.clear();
    BaseRenderer::endFrame();
}

vulkan::VulkanRenderer::~VulkanRenderer()
{
    presentThread.request_stop();
    presentThread.join();
    context.device.waitIdle();
    ImGui_ImplVulkan_Shutdown();
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
    context.device.destroy(descriptorPool);
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
        pipelineFactory.get(),
        [this](const Texture* texture) {
            for (const Frame& frame : frames)
                texture->writeDescriptor(frame.frameSet, frame.textureBinding);
        }
    );
}

void vulkan::VulkanRenderer::computePrePass(const uint32_t drawCount, const bool cull)
{
    const Frame& frame = getCurrentFrame();
    const vk::CommandBuffer cmd = frame.cmd;
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        cullPipeline.getLayout(),
        0,
        frame.computeSet,
        {}
    );
    cullPipeline.bind(cmd);
    uint32_t baseIndex = 0;
    for (const auto& info : std::ranges::views::values(pipelineMap)) {
        if (info.drawCount == 0)
            continue;
        const uint32_t pushConstants[] = {baseIndex, info.index, drawCount, cull ? 1u : 0};
        cmd.pushConstants(
            cullPipeline.getLayout(),
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
    transitionImageLayout(
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
    if (context.sampleCount == vk::SampleCountFlagBits::e1) {
        color.imageView = swapchain.currentView();
        color.loadOp = vk::AttachmentLoadOp::eClear;
        color.storeOp = vk::AttachmentStoreOp::eStore;
        color.resolveMode = vk::ResolveModeFlagBits::eNone;
    }
    else {
        transitionImageLayout(
            msaaImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits::eNone,
            vk::AccessFlagBits::eColorAttachmentWrite,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        );
        color.imageView = msaaView;
        color.loadOp = vk::AttachmentLoadOp::eClear;
        color.storeOp = vk::AttachmentStoreOp::eStore;
        color.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        color.resolveImageView = swapchain.currentView();
        color.resolveMode = vk::ResolveModeFlagBits::eAverage;
    }

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
                recreateSwapchain(vsync);
                break;
            }
            default: crash("Failed to acquire next swapchain image: {}", to_string(result));
        }
        result = swapchain.next(frame.renderingSemaphore);
        retries++;
    } while (result != vk::Result::eSuccess);
    context.device.resetFences(frame.fence);
    context.device.resetCommandPool(frame.pool);
    constexpr vk::CommandBufferBeginInfo beginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    frame.cmd.begin(beginInfo);
}

void vulkan::VulkanRenderer::writeGlobalUBO(const Camera& camera) const
{
    auto ptr = static_cast<char*>(globalUBO.getInfo().pMappedData);
    ptr += uboOffset * (getFrameCount() % FRAMES_IN_FLIGHT);
    const auto data = reinterpret_cast<UBOData*>(ptr);
    const glm::mat4 view = camera.getViewMatrix();
    data->orthographic = camera.orthograhpic;
    data->perspective = camera.perspective;
    data->view = view;
    data->resolution = glm::vec2(swapchain.getExtent().width, swapchain.getExtent().height);
    data->cameraPosition = camera.position;
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

        context.queues.graphics.submit2(submitInfo, presentData.frame->fence);

        vk::PresentInfoKHR presentInfo{};
        vk::SwapchainKHR swapchainKhr = swapchain;
        presentInfo.pSwapchains = &swapchainKhr;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &presentData.imageIndex;
        presentInfo.pWaitSemaphores = &presentData.frame->presentSemaphore;
        presentInfo.waitSemaphoreCount = 1;
        presentData.result = context.queues.present.presentKHR(&presentInfo);
        presentData.frame = nullptr;
        lock.unlock();
        presentData.condVar.notify_one();
    }
    logger->trace("Presentation thread destroyed");
}

void vulkan::VulkanRenderer::transitionImageLayout(
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

void vulkan::VulkanRenderer::initImages()
{
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.priority = 1.0f;
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

vulkan::Pipeline vulkan::VulkanRenderer::createComputePipeline() const
{
    PipelineInfo info{};
    info.type = PipelineInfo::Type::COMPUTE;
    info.vertexCompShader = "cull.comp";
    const auto pipeline = pipelineFactory->getOrCreate(info);
    return pipeline;
}

void vulkan::VulkanRenderer::recreateSwapchain(const bool vsync)
{
    context.device.waitIdle();
    swapchain = Swapchain(getWindow(), context, vsync, swapchain);
    context.device.destroy(msaaView);
    context.device.destroy(depthView);
    initImages();
}

void vulkan::VulkanRenderer::initImGui()
{
    if (!ImGui_ImplVulkan_LoadFunctions(&Context::getFunctionByName))
        crash("Failed to load imGui functions");

    ImGui_ImplSDL2_InitForVulkan(getWindow());
    context.queues.graphics.waitIdle();
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.UseDynamicRendering = true;
    initInfo.Device = context.device;
    initInfo.Instance = context.instance;
    initInfo.Queue = context.queues.graphics;
    initInfo.QueueFamily = context.queues.graphicsFamily;
    initInfo.Subpass = 0;
    initInfo.ImageCount = swapchain.getImageCount();
    initInfo.MinImageCount = FRAMES_IN_FLIGHT;
    initInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(context.sampleCount);
    initInfo.PhysicalDevice = context.physicalDevice;
    initInfo.PipelineCache = pipelineFactory->getCache();
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat
        = static_cast<VkFormat>(getDepthFormat(context.physicalDevice));
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.DescriptorPool = descriptorPool;
    const VkFormat swapchainFormat = static_cast<VkFormat>(swapchain.getFormat());
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;
    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
    logger->info("ImGui rendering initialized");
}

vulkan::VulkanRenderer::Frame::Frame(
    const Context& ctx,
    const vk::DescriptorPool descriptorPool,
    const GpuAllocator& allocator,
    const uint32_t maxDrawCount,
    const uint32_t index,
    const Buffer& globalUBO,
    const vk::DeviceSize uboOffset,
    DescriptorLayoutManager& descriptorLayoutManager
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

    createDescriptorSets(ctx, descriptorPool, descriptorLayoutManager, maxDrawCount);
    writeDescriptorsSets(ctx.device, index, maxDrawCount, globalUBO, uboOffset);
}

void vulkan::VulkanRenderer::Frame::createDescriptorSets(
    const Context& ctx,
    const vk::DescriptorPool descriptorPool,
    DescriptorLayoutManager& descriptorLayoutManager,
    const uint32_t maxDrawCount
)
{
    SmallVector<vk::DescriptorSetLayout> setLayouts;
    std::array<vk::DescriptorSetLayoutBinding, 6> layoutBindings{};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    layoutBindings[3].binding = 3;
    layoutBindings[3].descriptorType = vk::DescriptorType::eUniformBuffer;
    layoutBindings[4].binding = 4;
    layoutBindings[4].descriptorType = vk::DescriptorType::eStorageBuffer;
    layoutBindings[5].binding = 5;
    layoutBindings[5].descriptorType = vk::DescriptorType::eStorageBuffer;

    layoutBindings[5].descriptorCount = layoutBindings[4].descriptorCount = layoutBindings[3].descriptorCount
        = layoutBindings[2].descriptorCount = layoutBindings[1].descriptorCount
        = layoutBindings[0].descriptorCount = 1;
    layoutBindings[5].stageFlags = layoutBindings[4].stageFlags = layoutBindings[3].stageFlags
        = layoutBindings[2].stageFlags = layoutBindings[1].stageFlags = layoutBindings[0].stageFlags
        = vk::ShaderStageFlagBits::eCompute;
    setLayouts.pushBack(descriptorLayoutManager.createLayout(layoutBindings));
    std::array globalBinding = {vk::DescriptorSetLayoutBinding{
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
    }};
    setLayouts.pushBack(descriptorLayoutManager.createLayout(globalBinding));
    std::array<vk::DescriptorSetLayoutBinding, 3> frameBindings{};
    frameBindings[0].binding = 0;
    frameBindings[0].descriptorCount = 1;
    frameBindings[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    frameBindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
    frameBindings[1].binding = 1;
    frameBindings[1].descriptorCount = 1;
    frameBindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    frameBindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;
    textureBinding = frameBindings[2].binding = 2;
    frameBindings[2].descriptorCount = maxDrawCount;
    frameBindings[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    frameBindings[2].stageFlags = vk::ShaderStageFlagBits::eFragment;

    std::array bindlessIndices = {2ul};

    setLayouts.pushBack(descriptorLayoutManager.createBindlessLayout(
        frameBindings,
        bindlessIndices,
        vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool
    ));

    vk::DescriptorSetVariableDescriptorCountAllocateInfo varAllocInfo{};
    varAllocInfo.descriptorSetCount = frameBindings.size();
    const std::array counts = {0u, 0u, maxDrawCount};
    varAllocInfo.pDescriptorCounts = counts.data();

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = setLayouts.size();
    allocInfo.pSetLayouts = setLayouts.data();
    allocInfo.pNext = &varAllocInfo;
    const auto sets = ctx.device.allocateDescriptorSets<FrameAllocator<vk::DescriptorSet>>(allocInfo);
    computeSet = sets[0];
    globalDescriptorSet = sets[1];
    frameSet = sets[2];
}

void vulkan::VulkanRenderer::Frame::writeDescriptorsSets(
    const vk::Device device,
    const uint32_t index,
    const uint32_t maxDrawCount,
    const Buffer& globalUBO,
    const vk::DeviceSize uboOffset
) const
{
    vk::DescriptorBufferInfo globalUboInfo{};
    globalUboInfo.buffer = globalUBO;
    globalUboInfo.offset = index * uboOffset;
    globalUboInfo.range = sizeof(UBOData);

    std::array<vk::WriteDescriptorSet, 9> writes{};
    writes[0].dstSet = globalDescriptorSet;
    writes[0].dstArrayElement = 0;
    writes[0].dstBinding = 0;
    writes[0].pBufferInfo = &globalUboInfo;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;

    vk::DescriptorBufferInfo drawBufInfo{};
    drawBufInfo.buffer = drawData;
    drawBufInfo.offset = 0;
    drawBufInfo.range = maxDrawCount * sizeof(DrawData);
    writes[1].dstSet = computeSet;
    writes[1].dstArrayElement = 0;
    writes[1].dstBinding = 4;
    writes[1].pBufferInfo = &drawBufInfo;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;

    vk::DescriptorBufferInfo culledMatrixInfo{};
    culledMatrixInfo.buffer = culledMatrices;
    culledMatrixInfo.offset = 0;
    culledMatrixInfo.range = maxDrawCount * sizeof(glm::mat4);
    writes[2].dstSet = computeSet;
    writes[2].dstArrayElement = 0;
    writes[2].dstBinding = 0;
    writes[2].pBufferInfo = &culledMatrixInfo;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = vk::DescriptorType::eStorageBuffer;

    vk::DescriptorBufferInfo commandInfo{};
    commandInfo.buffer = commandBuffer;
    commandInfo.offset = 0;
    commandInfo.range = maxDrawCount * sizeof(vk::DrawIndexedIndirectCommand);
    writes[3].dstSet = computeSet;
    writes[3].dstArrayElement = 0;
    writes[3].dstBinding = 1;
    writes[3].pBufferInfo = &commandInfo;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = vk::DescriptorType::eStorageBuffer;

    vk::DescriptorBufferInfo countInfo{};
    countInfo.buffer = countBuffer;
    countInfo.offset = 0;
    countInfo.range = maxDrawCount * sizeof(uint32_t);
    writes[4].dstSet = computeSet;
    writes[4].dstArrayElement = 0;
    writes[4].dstBinding = 2;
    writes[4].pBufferInfo = &countInfo;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[5].dstSet = computeSet;
    writes[5].dstArrayElement = 0;
    writes[5].dstBinding = 3;
    writes[5].pBufferInfo = &globalUboInfo;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType = vk::DescriptorType::eUniformBuffer;

    vk::DescriptorBufferInfo textureIndexBuffInfo{};
    textureIndexBuffInfo.buffer = textureIndexBuffer;
    textureIndexBuffInfo.offset = 0;
    textureIndexBuffInfo.range = maxDrawCount * sizeof(TextureIds);
    writes[6].dstSet = computeSet;
    writes[6].dstArrayElement = 0;
    writes[6].dstBinding = 5;
    writes[6].pBufferInfo = &textureIndexBuffInfo;
    writes[6].descriptorCount = 1;
    writes[6].descriptorType = vk::DescriptorType::eStorageBuffer;

    vk::DescriptorBufferInfo frameInfo{};
    frameInfo.range = maxDrawCount * sizeof(glm::mat4);
    frameInfo.offset = 0;
    frameInfo.buffer = culledMatrices;
    writes[7].dstSet = frameSet;
    writes[7].dstArrayElement = 0;
    writes[7].dstBinding = 0;
    writes[7].pBufferInfo = &frameInfo;
    writes[7].descriptorCount = 1;
    writes[7].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[8].dstSet = frameSet;
    writes[8].dstArrayElement = 0;
    writes[8].dstBinding = 1;
    writes[8].pBufferInfo = &textureIndexBuffInfo;
    writes[8].descriptorCount = 1;
    writes[8].descriptorType = vk::DescriptorType::eStorageBuffer;

    device.updateDescriptorSets(writes, {});
}
}// namespace dragonfire