//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <Interface.h>

namespace dragonfire::rendering {
class RenderingEngine : public IRenderEngine {
public:
    RenderingEngine(SDL_Window* window, bool validation);

private:
    /// callback to log debug messages from the validation layers
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData) noexcept;

private:
    vk::Instance instance;

};
}   // namespace dragonfire::rendering