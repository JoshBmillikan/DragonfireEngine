//
// Created by josh on 1/15/24.
//

#pragma once
#include "vulkan_headers.h"
#include <SDL_video.h>
#include <memory>

namespace dragonfire::vulkan {

struct Context {
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
    vk::SurfaceKHR surface;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

    struct Queues {
        uint32_t graphicsFamily = 0, presentFamily = 0, transferFamily = 0;
        vk::Queue graphics, present, transfer;
    } queues;

    std::unique_ptr<vk::PhysicalDeviceProperties> deviceProperties;

    Context() = default;
    Context(
        SDL_Window* window,
        std::span<const char*> enabledExtensions,
        const vk::PhysicalDeviceFeatures2& requiredFeatures,
        bool enableValidation
    );
    void destroy();

    static PFN_vkVoidFunction getFunctionByName(const char* functionName, void*) noexcept;
};

}// namespace dragonfire::vulkan
