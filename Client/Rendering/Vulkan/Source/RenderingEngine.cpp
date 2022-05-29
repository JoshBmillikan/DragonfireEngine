//
// Created by Josh on 5/14/2022.
//

#include "RenderingEngine.h"
#include "Allocation.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
using namespace dragonfire::rendering;
/// Waits for render fences and starts rendering using the dynamic rendering extension
void RenderingEngine::beginRendering(const glm::mat4& view, const glm::mat4& projection) noexcept {
    lastRenderedMaterial = nullptr;
    lastRenderedMesh = nullptr;
    auto& frame = getCurrentFrame();
    if (device.waitForFences(frame.fence, true, UINT64_MAX) != vk::Result::eSuccess)
        spdlog::error("Main render fence wait failed");
    device.resetFences(frame.fence);
    auto [result, index] = device.acquireNextImageKHR(swapchain, UINT64_MAX, frame.presentSemaphore);
    vk::resultCheck(result, "Failed to acquire next swapchain image");
    currentImageIndex = index;
    frame.ubo->view = view;
    frame.ubo->projection = projection;
    device.resetCommandPool(frame.primaryPool);
    for (auto pool : frame.secondaryPools)
        device.resetCommandPool(pool);
    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    frame.cmd.begin(beginInfo);

    vk::RenderingAttachmentInfo attachmentInfo {
            .imageView = views[currentImageIndex],
            .imageLayout = vk::ImageLayout::eAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearValue,
    };

    for (auto i = 0; i < frame.secondaryBuffers.size(); i++) {
        device.resetCommandPool(frame.secondaryPools[i]);
        vk::CommandBufferInheritanceRenderingInfo renderingInfo{
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &surfaceFormat.format,
                // todo depth buffer, view mask
        };

        vk::CommandBufferInheritanceInfo inheritanceInfo{
                .pNext = &renderingInfo,
                .renderPass = nullptr,
                .framebuffer = nullptr,
        };

        vk::CommandBufferBeginInfo begin2{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
                         | vk::CommandBufferUsageFlagBits::eRenderPassContinue,
                .pInheritanceInfo = &inheritanceInfo,
        };
        frame.secondaryBuffers[i].begin(begin2);
    }

    vk::RenderingInfo renderInfo{
            .flags = vk::RenderingFlagBits::eContentsSecondaryCommandBuffers,
            .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = swapchainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentInfo,
    };
    frame.cmd.beginRendering(renderInfo);
    for (auto& q : threadQueues)
        q.wait_enqueue(RenderCommand());
}

void RenderingEngine::render(BaseMesh* meshPtr, IMaterial* materialPtr, const glm::mat4& transform) noexcept {
    static uint32_t currentThread = 0;
    auto mesh = static_cast<Mesh*>(meshPtr);               // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    auto material = static_cast<Material*>(materialPtr);   // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

    // If the mesh or material is the same as the last one, use the same thread to allow for
    // batching/better resource usage, otherwise distribute work in a round-robin style
    if (!(mesh == lastRenderedMesh || material == lastRenderedMaterial))
        currentThread = (currentThread + 1) % renderThreadCount;
    RenderCommand command{
            .state = RenderCommand::RenderState::Render,
            .mesh = mesh,
            .material = material,
            .transform = &transform,
    };
    threadQueues[currentThread].wait_enqueue(command);
    lastRenderedMesh = mesh;
    lastRenderedMaterial = material;
}

void RenderingEngine::endRendering() noexcept {
    barrier.arrive_and_wait();
    auto& frame = getCurrentFrame();
    for (auto cmd : frame.secondaryBuffers)
        cmd.end();
    auto cmd = frame.cmd;
    cmd.executeCommands(frame.secondaryBuffers);
    cmd.endRendering();
    cmd.end();
    frame.cmd.endRendering();
    frame.cmd.end();

    std::unique_lock lock(presentMutex);
    assert(!presentData.has_value());
    presentData.emplace(frame, currentImageIndex);
    lock.unlock();
    presentCond.notify_one();
    frameCount++;
}

void RenderingEngine::renderThread(const std::stop_token& token, const uint32_t index) noexcept {
    Material* lastMaterial = nullptr;
    Mesh* lastMesh = nullptr;
    vk::CommandBuffer cmd = nullptr;
    while (!token.stop_requested()) {
        RenderCommand command;
        threadQueues[index].wait_dequeue(command);

        switch (command.state) {
            case RenderCommand::RenderState::Start:
                lastMaterial = nullptr;
                lastMesh = nullptr;
                cmd = getCurrentFrame().secondaryBuffers[index];
                break;
            case RenderCommand::RenderState::Render: {
                auto mesh = command.mesh;
                auto material = command.material;
                auto transform = command.transform;

                auto layout = material->getLayout();
                cmd.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), transform);

                if (mesh != lastMesh) {
                    lastMesh = mesh;
                    mesh->bind(cmd);
                }

                if (material != lastMaterial) {
                    lastMaterial = material;
                    material->bind(cmd);
                }

                cmd.drawIndexed(mesh->indices.size(), 1, 0, 0, 0);

            } break;
            case RenderCommand::RenderState::End:
                if (token.stop_requested())
                    barrier.arrive_and_drop();
                else
                    barrier.arrive_and_wait();
                break;
        }
    }
}

void RenderingEngine::present(const std::stop_token& token) noexcept {
    while (!token.stop_requested()) {
        std::unique_lock lock(presentMutex);
        presentCond.wait(lock, token, [&] { return presentData.has_value(); });
        if (token.stop_requested())
            break;

        vk::SubmitInfo submitInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &presentData->frame.presentSemaphore,
                .commandBufferCount = 1,
                .pCommandBuffers = &presentData->frame.cmd,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &presentData->frame.renderSemaphore,
        };

        uint32_t index = presentData->imageIndex;
        vk::PresentInfoKHR presentInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &presentData->frame.renderSemaphore,
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &index,
        };
        presentData.reset();
        lock.unlock();

        graphicsQueue.submit(submitInfo);
        if (presentationQueue.presentKHR(presentInfo) != vk::Result::eSuccess) {
            spdlog::error("Swapchain presentation failed");
            //todo handle this
        }
    }
}

void RenderingEngine::resize(uint32_t width, uint32_t height) {
    spdlog::info("Resized window to {}x{}", width, height);
    // todo
}

RenderingEngine::~RenderingEngine() {
    device.waitIdle();
    for (auto& thread : renderThreads)
        thread.request_stop();
    for (auto& q : threadQueues)
        q.wait_enqueue(RenderCommand{.state = RenderCommand::RenderState::End});
    presentationThread.request_stop();
    presentationThread.join();
    barrier.arrive_and_drop();
    renderThreads.clear();

    device.destroy(utilityPool);
    for (auto& frame : frames) {
        frame.ubo.reset();
        device.destroy(frame.primaryPool);
        device.destroy(frame.fence);
        device.destroy(frame.presentSemaphore);
        device.destroy(frame.renderSemaphore);
        for (auto pool : frame.secondaryPools)
            device.destroy(pool);
    }

    for(auto& view : views)
        device.destroy(view);
    device.destroy(swapchain);
    Allocation::destroyAllocator();
    device.destroy();
    instance.destroy(surface);
    instance.destroy();
}

VKAPI_ATTR VkBool32 VKAPI_CALL RenderingEngine::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        [[maybe_unused]] void* pUserData
) noexcept {
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error("Validation Layer: {}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn("Validation Layer: {}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            spdlog::info("Validation Layer: {}", pCallbackData->pMessage);
            break;
        default:
            spdlog::trace("Validation Layer: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}
