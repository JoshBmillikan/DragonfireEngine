//
// Created by josh on 1/15/24.
//

#define VULKAN_HPP_USE_REFLECT
#include "context.h"
#include <SDL_vulkan.h>
#include <core/crash.h>
#include <spdlog/spdlog.h>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace dragonfire::vulkan {

/***
 * @brief Initializes the vulkan dynamic loader
 */
static void initVulkan() noexcept
{
    const auto logger = spdlog::get("Rendering");
    void* proc = SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (proc == nullptr)
        crash("Failed to get vulkan instance proc address: {}", SDL_GetError());
    VULKAN_HPP_DEFAULT_DISPATCHER.init(PFN_vkGetInstanceProcAddr(proc));
    try {
        const uint32_t version = vk::enumerateInstanceVersion();
        logger->info(
            "Vulkan version {:d}.{:d}.{:d} loaded (header version {:d}.{:d}.{:d})",
            VK_API_VERSION_MAJOR(version),
            VK_API_VERSION_MINOR(version),
            VK_API_VERSION_PATCH(version),
            VK_API_VERSION_MAJOR(VK_HEADER_VERSION),
            VK_API_VERSION_MINOR(VK_HEADER_VERSION),
            VK_API_VERSION_PATCH(VK_HEADER_VERSION)
        );
    }
    catch (...) {
        logger->error("Unknown vulkan version loaded");
    }
}

/***
 * Function to log validation layer messages,
 * Used as a callback in the debug create info struct
 */
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    const auto logger = static_cast<spdlog::logger*>(pUserData);
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            logger->error("Validation Layer: {}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            logger->warn("Validation Layer: {}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            logger->info("Validation Layer: {}", pCallbackData->pMessage);
            break;
        default: logger->trace("Validation Layer: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

/***
 * @brief Fills in the debug messenger create info struct
 * @param logger Logger pointer to pass to the debug callback
 * @return The debug create info struct
 */
static vk::DebugUtilsMessengerCreateInfoEXT getDebugCreateInfo(spdlog::logger* logger)
{
    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
                                 | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                 | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                 | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                             | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                             | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    createInfo.pfnUserCallback = &debugCallback;
    createInfo.pUserData = logger;
    return createInfo;
}

/***
 * @brief Creates the vulkan instance
 * @param window The SDL window handle
 * @param validation If true, enable validation layers for instance creation
 * @return The created instance
 */
static vk::Instance createInstance(SDL_Window* window, const bool validation)
{
    uint32_t windowExtensionCount;
    const char** extensions = nullptr;
    if (SDL_Vulkan_GetInstanceExtensions(window, &windowExtensionCount, extensions) == SDL_FALSE)
        crash("SDL failed to get instance extensions: {}", SDL_GetError());

    uint32_t extensionCount = windowExtensionCount;
    if (validation)
        extensionCount++;

    extensions = static_cast<const char**>(alloca(extensionCount * sizeof(const char*)));

    if (SDL_Vulkan_GetInstanceExtensions(window, &windowExtensionCount, extensions) == SDL_FALSE)
        crash("SDL failed to get instance extensions: {}", SDL_GetError());

    if (validation)
        extensions[extensionCount - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    const auto logger = spdlog::get("Rendering");
    for (uint32_t i = 0; i < extensionCount; i++)
        logger->info("Loaded instance extension \"{}\"", extensions[i]);

    uint32_t layerCount = 0;
    const char* layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    if (validation)
        layerCount++;

    for (uint32_t i = 0; i < layerCount; i++)
        logger->info("Loaded layer \"{}\"", layers[i]);

    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName = APP_NAME;
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "no engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.ppEnabledLayerNames = layers;
    createInfo.enabledLayerCount = layerCount;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = extensionCount;

    vk::DebugUtilsMessengerCreateInfoEXT debug;
    if (validation) {
        debug = getDebugCreateInfo(logger.get());
        createInfo.pNext = &debug;
    }

    const vk::Instance instance = vk::createInstance(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
    return instance;
}

/// Creates the surface handle for the SDL window
static vk::SurfaceKHR createSurface(SDL_Window* window, const vk::Instance instance) noexcept
{
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, instance, &surface) == SDL_FALSE)
        crash("SDL Failed to create surface");
    return surface;
}

/***
 * @brief Attempt to log the GPU driver version
 *  The version exposed by vulkan is a packed 32 bit integer, in most cases
 *  this can be decoded using VK_VERSION macros but some vendors use a different
 * packing. We make a best effort attempt to accommodate this, but it may not
 * always be accurate.
 * @param properties physical device properties to get the driver version/vendor id from
 */
static void logDriverVersion(const vk::PhysicalDeviceProperties& properties)
{
    const auto logger = spdlog::get("Rendering");
    const uint32_t version = properties.driverVersion;
    switch (properties.vendorID) {
        case 4318:// NVIDIA
            logger->info(
                "GPU driver version {}.{}.{}.{}",
                version >> 22 & 0x3ff,
                version >> 14 & 0x0ff,
                version >> 6 & 0x0ff,
                version & 0x003f
            );
            break;
        case 0x8086:// INTEL
#ifdef _WIN32       // Intel only uses a different scheme on windows
            logger->info("GPU driver version {}.{}", version >> 14, version & 0x3fff);
            break;
#endif
        default:
            logger->info(
                "GPU driver version {}.{}.{}",
                VK_VERSION_MAJOR(version),
                VK_VERSION_MINOR(version),
                VK_VERSION_PATCH(version)
            );
    }
}

template<typename T, size_t INDEX = 0>
    requires(INDEX < std::tuple_size_v<T>)
static bool checkFeatures(const T& requried, const T& supported)
{
    bool ok = std::get<INDEX>(requried) ? std::get<INDEX>(supported) : true;
    return ok && checkFeatures<T, INDEX + 1>(requried, supported);
}

template<typename T, size_t INDEX = 0>
static bool checkFeatures(const T& requried, const T& supported)
{
    return true;
}


static bool supportsRequriedFeatures(
    vk::PhysicalDevice device,
    const vk::PhysicalDeviceFeatures2& requiredFeatures
)
{
    auto supported = device.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceVulkan13Features>();
    auto& base = supported.get<vk::PhysicalDeviceFeatures2>().features;
    checkFeatures(requiredFeatures.features.reflect(), base.reflect());
}

/***
 * @brief Gets the queue family indices for the graphics, present, and transfer queues
 *
 * Attempts to get distinct present and transfer queues if available, but will fall back to
 * using the graphics queue if no dedicated queue exists and the graphics queue supports those
 * operations.
 *
 * @param physicalDevice Physical device to get the queues for
 * @param surface Surface the present queue must support
 * @param queues Pointer to the queues struct, if null will only check that all required queues
 * are supported for the device/surface, otherwise will fill in this struct with the queue families
 * found.
 * @return true if all the required queues are supported for the device and surface, otherwise false
 */
static bool getQueueFamilies(
    const vk::PhysicalDevice physicalDevice,
    const vk::SurfaceKHR surface,
    Context::Queues* queues = nullptr
) noexcept
{
    uint32_t count;
    vk::QueueFamilyProperties* properties = nullptr;
    physicalDevice.getQueueFamilyProperties(&count, properties);
    properties = static_cast<vk::QueueFamilyProperties*>(alloca(sizeof(vk::QueueFamilyProperties) * count));
    physicalDevice.getQueueFamilyProperties(&count, properties);

    bool foundPresent, foundTransfer;
    bool foundGraphics = foundPresent = foundTransfer = false;
    for (uint32_t i = 0; i < count; i++) {
        auto& prop = properties[i];
        vk::Bool32 surfaceSupport;
        if (physicalDevice.getSurfaceSupportKHR(i, surface, &surfaceSupport) != vk::Result::eSuccess)
            surfaceSupport = false;

        if (!foundGraphics
            && prop.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) {
            foundGraphics = true;
            if (queues)
                queues->graphicsFamily = i;
        }
        else if (!foundTransfer && prop.queueFlags & (vk::QueueFlagBits::eTransfer)) {
            foundTransfer = true;
            if (queues)
                queues->transferFamily = i;
        }
        if (!foundPresent && surfaceSupport) {
            foundPresent = true;
            if (queues)
                queues->presentFamily = i;
        }

        if (foundGraphics && foundPresent && foundTransfer)
            break;
    }
    if (queues && foundGraphics && !foundTransfer)
        queues->transferFamily = queues->graphicsFamily;

    return foundGraphics && foundPresent;
}

/***
 * @brief Checks if the given device handle supports everything we require
 * @param device Device to check
 * @param surface Surface that will be used with the device
 * @param requiredFeatures Required device features
 * @param enabledExtensions Device extensions that must be supported
 * @return true if the device is valid for our requirements, false if not
 */
static bool isValidDevice(
    const vk::PhysicalDevice device,
    const vk::SurfaceKHR surface,
    const vk::PhysicalDeviceFeatures2& requiredFeatures,
    const std::span<const char*> enabledExtensions
)
{
    if (!supportsRequriedFeatures(device, requiredFeatures))
        return false;
    vk::ExtensionProperties* properties = nullptr;
    uint32_t count = 0;
    if (device.enumerateDeviceExtensionProperties(nullptr, &count, properties) != vk::Result::eSuccess)
        return false;
    properties = static_cast<vk::ExtensionProperties*>(alloca(sizeof(vk::ExtensionProperties) * count));
    if (device.enumerateDeviceExtensionProperties(nullptr, &count, properties) != vk::Result::eSuccess)
        return false;

    for (const char* extension : enabledExtensions) {
        bool found = false;
        for (uint32_t i = 0; i < count; i++) {
            if (strcmp(extension, properties[i].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    return getQueueFamilies(device, surface);
}

static vk::PhysicalDevice getPhysicalDevice(
    const vk::Instance instance,
    const vk::SurfaceKHR surface,
    const vk::PhysicalDeviceFeatures2& requiredFeatures,
    const std::span<const char*> enabledExtensions,
    vk::PhysicalDeviceProperties* deviceProperties
)
{
    uint32_t deviceCount;
    vk::PhysicalDevice* devices = nullptr;
    if (instance.enumeratePhysicalDevices(&deviceCount, devices) != vk::Result::eSuccess)
        crash("Failed to enumerate physical devices");
    devices = static_cast<vk::PhysicalDevice*>(alloca(deviceCount * sizeof(vk::PhysicalDevice)));
    if (instance.enumeratePhysicalDevices(&deviceCount, devices) != vk::Result::eSuccess)
        crash("Failed to enumerate physical devices");

    for (uint32_t i = 0; i < deviceCount; i++) {}
}

Context::Context(
    SDL_Window* window,
    std::span<const char*> enabledExtensions,
    std::span<const char*> optionalExtensions,
    bool enableValidation
)
{
    enableValidation = enableValidation || std::getenv("VALIDATION_LAYERS");
    initVulkan();
    instance = createInstance(window, enableValidation);
    const auto logger = spdlog::get("Rendering");
    if (enableValidation) {
        auto createInfo = getDebugCreateInfo(logger.get());
        debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
        logger->info("Vulkan validation layers enabled");
    }
    surface = createSurface(window, instance);

    logger->info("Vulkan context initialized");
}
}// namespace dragonfire::vulkan
