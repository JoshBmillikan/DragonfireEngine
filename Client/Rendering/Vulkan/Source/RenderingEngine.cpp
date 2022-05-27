//
// Created by Josh on 5/14/2022.
//

#include "RenderingEngine.h"
#include "Allocation.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
using namespace dragonfire::rendering;

void RenderingEngine::beginRendering(const glm::mat4& view, const glm::mat4& projection) noexcept {
    lastRenderedMaterial = nullptr;
    lastRenderedMesh = nullptr;
    auto& frame = getCurrentFrame();
    device.resetCommandPool(frame.primaryPool);
    for (auto pool : frame.secondaryPools)
        device.resetCommandPool(pool);
    vk::CommandBufferBeginInfo beginInfo{

    };
    frame.cmd.begin(beginInfo);
    vk::RenderingInfo renderInfo{

    };
    frame.cmd.beginRendering(renderInfo);
    for(auto& q : threadQueues)
        q.wait_enqueue(RenderCommand());
}

void RenderingEngine::render(BaseMesh* meshPtr, IMaterial* materialPtr, const glm::mat4& transform) noexcept {
    static uint32_t currentThread = 0;
    auto mesh = static_cast<Mesh*>(meshPtr);               // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    auto material = static_cast<Material*>(materialPtr);   // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    if (!(mesh == lastRenderedMesh || material == lastRenderedMaterial))
        currentThread = (currentThread + 1) % renderThreadCount;
    RenderCommand command {
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
    auto& frame = getCurrentFrame();
    for(auto cmd : frame.secondaryBuffers)
        cmd.end();
    auto cmd = frame.cmd;
    cmd.executeCommands(frame.secondaryBuffers);
    cmd.endRendering();
    cmd.end();
    frame.cmd.endRendering();
    frame.cmd.end();
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
                    //todo bind material
                }

                cmd.drawIndexed(mesh->indices.size(), 1, 0, mesh->vertexOffset, 0);

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

void RenderingEngine::present(const std::stop_token& token) noexcept {}

RenderingEngine::~RenderingEngine() {
    for (auto& thread : renderThreads)
        thread.request_stop();
    for (auto& q : threadQueues)
        q.wait_enqueue(RenderCommand{.state = RenderCommand::RenderState::End});
    barrier.arrive_and_drop();
    renderThreads.clear();
    Allocation::destroyAllocator();
    device.destroy();
    instance.destroy(surface);
    instance.destroy();
}

void RenderingEngine::resize(uint32_t width, uint32_t height) {
    spdlog::info("Resized window to {}x{}", width, height);
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
