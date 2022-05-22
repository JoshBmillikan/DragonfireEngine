//
// Created by Josh on 5/14/2022.
//
#include "Allocation.h"
#include "RenderingEngine.h"
#include <Engine.h>
#include <SDL_vulkan.h>

static vk::Instance createInstance(SDL_Window* window, PFN_vkDebugUtilsMessengerCallbackEXT debugCallback);

static vk::PhysicalDevice getPhysicalDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& deviceExtensions
);

static vk::SurfaceKHR createSurface(SDL_Window* window, vk::Instance instance);

inline static std::vector<const char*> getLayers(bool validation) {
    std::vector<const char*> layers{};
    if (validation)
        layers.push_back("VK_LAYER_KHRONOS_validation");
    return layers;
}

struct QueueFamilies {
    uint32_t graphicsIndex, presentationIndex;

    QueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);

    [[nodiscard]] inline bool isUnified() const noexcept { return graphicsIndex == presentationIndex; }
};

static vk::Device createDevice(
        vk::PhysicalDevice physicalDevice,
        const std::vector<const char*>& extensions,
        const QueueFamilies& queueFamilies
);

dragonfire::rendering::RenderingEngine::RenderingEngine(SDL_Window* window, bool validation) {
    assert(window);
    instance = createInstance(window, validation ? RenderingEngine::debugCallback : nullptr);
    surface = createSurface(window, instance);

    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    physicalDevice = getPhysicalDevice(instance, surface, deviceExtensions);
    spdlog::info("Using GPU {}", physicalDevice.getProperties().deviceName);
    QueueFamilies queueFamilies(physicalDevice, surface);
    device = createDevice(physicalDevice, deviceExtensions, queueFamilies);
    Allocation::initAllocator(instance, physicalDevice, device);

    graphicsQueue = device.getQueue(queueFamilies.graphicsIndex, 0);
    presentationQueue = device.getQueue(queueFamilies.presentationIndex, 0);

    auto threadCount = std::max(std::thread::hardware_concurrency() / 2, 1u);
    renderThreads.reserve(threadCount);
    for (auto i = 0; i < threadCount; i++)
        renderThreads.emplace_back(std::bind_front(&dragonfire::rendering::RenderingEngine::renderThread, this));
    presentationThread = std::jthread(std::bind_front(&dragonfire::rendering::RenderingEngine::presentationThread, this));
}

QueueFamilies::QueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    auto props = device.getQueueFamilyProperties();
    bool foundGraphics = false, foundPresent = false;
    for (auto i = 0; i < props.size(); i++) {
        if (props[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            graphicsIndex = i;
            foundGraphics = true;
        }

        if (device.getSurfaceSupportKHR(i, surface)) {
            presentationIndex = i;
            foundPresent = true;
        }

        if (foundGraphics && foundPresent)
            break;
    }
    if (!foundGraphics)
        throw std::runtime_error("No graphics queue found");
    if (!foundPresent)
        throw std::runtime_error("No presentation queue found");
}

static vk::Device createDevice(
        vk::PhysicalDevice physicalDevice,
        const std::vector<const char*>& extensions,
        const QueueFamilies& queueFamilies
) {
    vk::DeviceQueueCreateInfo queueInfos[2];
    const float priority[2] = {1, 1};

    queueInfos[0].queueCount = 1;
    queueInfos[0].pQueuePriorities = priority;
    queueInfos[0].queueFamilyIndex = queueFamilies.graphicsIndex;
    if (!queueFamilies.isUnified()) {
        queueInfos[1] = queueInfos[0];
        queueInfos[1].queueFamilyIndex = queueFamilies.presentationIndex;
    }

    vk::DeviceCreateInfo createInfo{
            .queueCreateInfoCount = queueFamilies.isUnified() ? 1u : 2u,
            .pQueueCreateInfos = queueInfos,
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
    };

    auto device = physicalDevice.createDevice(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
    return device;
}

static bool isValidDevice(
        vk::PhysicalDevice device,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& requestedExtensions
) {
    auto props = device.getProperties();
    auto features = device.getFeatures();
    auto extProps = device.enumerateDeviceExtensionProperties(nullptr);

    // check if device supports all requested extensions
    if (!std::ranges::all_of(requestedExtensions, [&](const auto& ext) {
            return std::ranges::any_of(extProps, [&](const auto& prop) {
                return strcmp(prop.extensionName, ext) == 0;
            });
        }))
        return false;

    try {
        QueueFamilies(device, surface);
    }
    catch (const std::runtime_error& e) {
        return false;
    }

    return true;   //todo check other stuff
}

static vk::PhysicalDevice getPhysicalDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& deviceExtensions
) {
    auto devices = instance.enumeratePhysicalDevices();
    for (const char* ext : deviceExtensions)
        spdlog::info("Loaded device extension {}", ext);

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
        // If no suitable gpu was found at all
        throw std::runtime_error("No suitable GPU available");
    }

    return *device;
}

static std::vector<const char*> getInstanceExtensions(SDL_Window* window, bool validation) {
    std::vector<const char*> extensions;
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

static vk::Instance createInstance(SDL_Window* window, PFN_vkDebugUtilsMessengerCallbackEXT debugCallback = nullptr) {
    vk::DynamicLoader dl;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    vk::ApplicationInfo appInfo{
            .pApplicationName = APP_NAME,
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName = "Dragonfire Engine",
            .engineVersion = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion = VK_API_VERSION_1_3,
    };

    auto extensions = getInstanceExtensions(window, debugCallback != nullptr);
    for (const auto ext : extensions)
        spdlog::info("Loaded instance extension: {}", ext);
    auto layers = getLayers(debugCallback != nullptr);
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
            VK_API_VERSION_PATCH(vkVersion)
    );

    return instance;
}