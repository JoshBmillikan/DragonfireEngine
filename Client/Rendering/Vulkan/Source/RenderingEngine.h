//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <Interface.h>
#include <thread>
#include "Mesh.h"
#include "Material.h"

namespace dragonfire::rendering {
class RenderingEngine final : public IRenderEngine {
public:
    RenderingEngine(SDL_Window* window, bool validation);
    ~RenderingEngine() override;

    void beginRendering(const glm::mat4& view, const glm::mat4& projection) noexcept override;
    void render(BaseMesh* meshPtr, IMaterial* materialPtr, const glm::mat4& transform) noexcept override;
    void endRendering() noexcept override;

    RenderingEngine(RenderingEngine&& other) = delete;
    RenderingEngine(RenderingEngine& other) = delete;
    RenderingEngine& operator=(RenderingEngine&& other) = delete;
    RenderingEngine& operator=(RenderingEngine& other) = delete;

private:
    void renderThread(const std::stop_token& token) noexcept;

    /// callback to log debug messages from the validation layers
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
    ) noexcept;

private:
    uint64_t frameCount = 0;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;
    vk::Queue graphicsQueue, presentationQueue;

    std::vector<std::jthread> renderThreads;

};
}   // namespace dragonfire::rendering