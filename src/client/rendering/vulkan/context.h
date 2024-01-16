//
// Created by josh on 1/15/24.
//

#pragma once
#include "vulkan_headers.h"
#include <SDL_video.h>

namespace dragonfire::vulkan {

struct Context {
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
    vk::SurfaceKHR surface;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;

    struct Queues {
        uint32_t graphicsFamily = 0, presentFamily = 0, transferFamily = 0;
        vk::Queue graphics, present, transfer;
    } queues;

    vk::PhysicalDeviceProperties deviceProperties;

    Context() = default;
    Context(
        SDL_Window* window,
        std::span<const char*> enabledExtensions,
        std::span<const char*> optionalExtensions = {},
        bool enableValidation = true
    );
};

}// namespace dragonfire::vulkan
