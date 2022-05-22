//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <Interface.h>
#include <thread>
#include "Mesh.h"
#include "Material.h"
#include "Swapchain.h"

namespace dragonfire::rendering {
class RenderingEngine final : public IRenderEngine {
public:
    RenderingEngine(SDL_Window* window, bool validation);
    ~RenderingEngine() override;

    void beginRendering(const glm::mat4& view, const glm::mat4& projection) noexcept override;
    void render(BaseMesh* meshPtr, IMaterial* materialPtr, const glm::mat4& transform) noexcept override;
    void endRendering() noexcept override;
    void resize(uint32_t width, uint32_t height) override;

    RenderingEngine(RenderingEngine&& other) = delete;
    RenderingEngine(RenderingEngine& other) = delete;
    RenderingEngine& operator=(RenderingEngine&& other) = delete;
    RenderingEngine& operator=(RenderingEngine& other) = delete;

private:
    void renderThread(const std::stop_token& token) noexcept;
    void present(const std::stop_token& token) noexcept;

    /// callback to log debug messages from the validation layers
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
    ) noexcept;

private:
    constexpr static int FRAMES_IN_FLIGHT = 2;
    struct Frame {
        vk::CommandBuffer cmd;
    }frames[FRAMES_IN_FLIGHT];

    inline Frame& getCurrentFrame() noexcept {
        return frames[frameCount % FRAMES_IN_FLIGHT];
    }

    enum class RenderState {
        Start,
        Render,
        End
    };
private:
    uint64_t frameCount = 0;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;
    vk::Queue graphicsQueue, presentationQueue;

    std::vector<std::jthread> renderThreads;
    std::jthread presentationThread;

    Swapchain swapchain;
};
}   // namespace dragonfire::rendering