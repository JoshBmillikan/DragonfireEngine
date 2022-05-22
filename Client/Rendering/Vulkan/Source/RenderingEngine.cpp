//
// Created by Josh on 5/14/2022.
//

#include "RenderingEngine.h"
#include "Allocation.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
using namespace dragonfire::rendering;

void RenderingEngine::beginRendering(const glm::mat4& view, const glm::mat4& projection) noexcept {
    auto& frame = getCurrentFrame();
    vk::CommandBufferBeginInfo beginInfo {

    };
    frame.cmd.begin(beginInfo);
    vk::RenderingInfo renderInfo {

    };
    frame.cmd.beginRendering(renderInfo);
}

void RenderingEngine::render(BaseMesh* meshPtr, IMaterial* materialPtr, const glm::mat4& transform) noexcept {
    auto mesh = static_cast<Mesh*>(meshPtr);               // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    auto material = static_cast<Material*>(materialPtr);   // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
}

void RenderingEngine::endRendering() noexcept {
    auto& frame = getCurrentFrame();
    frame.cmd.endRendering();
    frame.cmd.end();
    frameCount++;
}

void RenderingEngine::renderThread(const std::stop_token& token) noexcept {
    Material* lastMaterial = nullptr;
    Mesh* lastMesh = nullptr;
    while(!token.stop_requested()) {

    }
}

void RenderingEngine::present(const std::stop_token& token) noexcept {}

RenderingEngine::~RenderingEngine() {
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
