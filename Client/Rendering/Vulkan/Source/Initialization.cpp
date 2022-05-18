//
// Created by Josh on 5/14/2022.
//
#include "RenderingEngine.h"
#include <Engine.h>
#include <SDL_vulkan.h>

constexpr size_t ARRAY_SIZE = 16;

static vk::Instance createInstance(
        SDL_Window* window,
        std::vector<const char*>& layers,
        PFN_vkDebugUtilsMessengerCallbackEXT debugCallback);

static vk::PhysicalDevice getPhysicalDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& deviceExtensions);
static vk::SurfaceKHR createSurface(SDL_Window* window, vk::Instance instance);

inline static std::vector<const char*> getLayers(bool validation) {
    std::vector<const char*> layers{};
    if (validation)
        layers.push_back("VK_LAYER_KHRONOS_validation");
    return layers;
}

dragonfire::rendering::RenderingEngine::RenderingEngine(SDL_Window* window, bool validation) {
    assert(window);
    auto layers = getLayers(validation);
    instance = createInstance(window, layers, validation ? RenderingEngine::debugCallback : nullptr);
    surface = createSurface(window, instance);

    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    physicalDevice = getPhysicalDevice(instance, surface, deviceExtensions);

    spdlog::info("Using GPU {}", physicalDevice.getProperties().deviceName);
}

static bool checkExtensionSupport(vk::PhysicalDevice device, const char* ext) {
    uint32_t propCount;

    vk::resultCheck(
            device.enumerateDeviceExtensionProperties(nullptr, &propCount, nullptr),
            "Failed to get device extension count");
    auto* extensionProps = (vk::ExtensionProperties*) alloca(propCount);

    vk::resultCheck(
            device.enumerateDeviceExtensionProperties(nullptr, &propCount, extensionProps),
            "Failed to get device extensions");

    for (uint32_t i = 0; i < propCount; i++) {
        if (strcmp(extensionProps->extensionName, ext) == 0)
            return true;
    }
    return false;
}

static bool isValidDevice(
        vk::PhysicalDevice device,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& requestedExtensions) {
    auto props = device.getProperties();
    auto features = device.getFeatures();

    // check if device supports all requested extensions
    for (const auto ext : requestedExtensions) {
        if (!checkExtensionSupport(device, ext))
            return false;
    }

    return true;   //todo check other stuff
}

static vk::PhysicalDevice getPhysicalDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& deviceExtensions) {

    auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty())
        throw std::runtime_error("No GPU found");

    auto device = std::find_if(devices.begin(), devices.end(), [&](const auto device) {
        return isValidDevice(device, surface, deviceExtensions)
        and device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
    });

    // find a valid integrated gpu if no discrete gpu was found
    if (device == devices.end()) {
        auto integrated = std::find_if(devices.begin(), devices.end(), [&](const auto device) {
            return isValidDevice(device, surface, deviceExtensions);
        });
        if (integrated != devices.end()) {
            spdlog::warn("No discrete GPU found, falling back to integrated gpu");
            return *integrated;
        }
    }

    throw std::runtime_error("No suitable GPU available");
}

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

static vk::SurfaceKHR createSurface(SDL_Window* window, vk::Instance instance) {
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, instance, &surface) == SDL_FALSE)
        throw std::runtime_error("SDL failed to create surface");
    return surface;
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
    for (const auto ext : extensions)
        spdlog::info("Loaded instance extension: {}", ext);
    for (const auto layer : layers)
        spdlog::info("Loaded layer: {}", layer);

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