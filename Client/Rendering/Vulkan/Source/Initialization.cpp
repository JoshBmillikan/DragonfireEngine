//
// Created by Josh on 5/14/2022.
//
#include "RenderingEngine.h"
#include <SDL_vulkan.h>
#include <Engine.h>

static std::vector<const char*> getInstanceExtensions(SDL_Window* window, bool validation) {
    std::vector<const char*> extensions{};
    unsigned int count;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
    extensions.resize(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data());
    if (validation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return extensions;
}


static vk::DebugUtilsMessengerCreateInfoEXT getDebugCreateInfo(PFN_vkDebugUtilsMessengerCallbackEXT callback) {
    return vk::DebugUtilsMessengerCreateInfoEXT{
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                               | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                               | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                           | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                           | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = callback};
}
static vk::Instance createInstance(
        SDL_Window* window,
        std::vector<const char*>& layers,
        PFN_vkDebugUtilsMessengerCallbackEXT debugCallback = nullptr) {
    vk::DynamicLoader dl;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    vk::ApplicationInfo appInfo{
            .pApplicationName = APP_NAME,
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName = "Dragonfire Engine",
            .engineVersion = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion = VK_API_VERSION_1_2,
    };
    auto extensions = getInstanceExtensions(window, debugCallback != nullptr);

    vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
    };
    vk::DebugUtilsMessengerCreateInfoEXT debug;
    if (debugCallback) {
        debug = getDebugCreateInfo(debugCallback);
        createInfo.pNext = &debug;
    }
    auto instance = vk::createInstance(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
    // log actual version of vulkan in use
    auto vkVersion = vk::enumerateInstanceVersion();
    spdlog::info(
            "Vulkan version: {:d}.{:d}.{:d} loaded",
            VK_API_VERSION_MAJOR(vkVersion),
            VK_API_VERSION_MINOR(vkVersion),
            VK_API_VERSION_PATCH(vkVersion));
    return instance;
}

static std::vector<const char*> getLayers(bool validation) {
    std::vector<const char*> layers{};
    if (validation)
        layers.push_back("VK_LAYER_KHRONOS_validation");
    return layers;
}

dragonfire::rendering::RenderingEngine::RenderingEngine(SDL_Window* window, bool validation) {
    assert(window);
    auto layers = getLayers(validation);
    instance = createInstance(window,layers, validation ? RenderingEngine::debugCallback : nullptr);
}
