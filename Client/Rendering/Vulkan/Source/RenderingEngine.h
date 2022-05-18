//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <Interface.h>

namespace dragonfire::rendering {
class RenderingEngine final : public IRenderEngine {
public:
    RenderingEngine(SDL_Window* window, bool validation);
    ~RenderingEngine() override;

    RenderingEngine(RenderingEngine&& other) = delete;
    RenderingEngine(RenderingEngine& other) = delete;
    RenderingEngine& operator = (RenderingEngine&& other) = delete;
    RenderingEngine& operator = (RenderingEngine& other) = delete;

private:
    /// callback to log debug messages from the validation layers
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData) noexcept;

private:
    uint64_t frameCount = 0;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;
};
}   // namespace dragonfire::rendering