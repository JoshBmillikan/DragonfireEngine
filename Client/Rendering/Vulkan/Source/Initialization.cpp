//
// Created by Josh on 5/14/2022.
//
#include "Allocation.h"
#include "RenderingEngine.h"
#include <Engine.h>
#include <SDL_vulkan.h>
#include <alloca.h>

/// \brief Creates the main vulkan instance
/// \param window a SDL window handle, used to retrieve the required vulkan extensions from SDL
/// \param debugCallback (optional) A debug callback function, used for enabling the validation layers
/// \return A vulkan instance handle
static vk::Instance createInstance(SDL_Window* window, PFN_vkDebugUtilsMessengerCallbackEXT debugCallback);

/// \brief Gets the physical gpu handle
/// \param instance A valid vulkan instance handle
/// \param surface A valid vulkan surface handle
/// \param deviceExtensions A vector of required extensions the device must support
/// \return A physical device handle to a device supporting the surface and given extensions
static vk::PhysicalDevice getPhysicalDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& deviceExtensions
);

/// \brief Creates the surface for the given SDL window
/// \param window A SDL window pointer used to create the surface
/// \param instance The vulkan instance handle
/// \return A vulkan surface handle for the given window
static vk::SurfaceKHR createSurface(SDL_Window* window, vk::Instance instance);

inline static std::vector<const char*> getLayers(bool validation) {
    std::vector<const char*> layers;
    if (validation)
        layers.push_back("VK_LAYER_KHRONOS_validation");
    return layers;
}

struct QueueFamilies {
    uint32_t graphicsIndex, presentationIndex;

    QueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);

    [[nodiscard]] inline bool isUnified() const noexcept { return graphicsIndex == presentationIndex; }
};

static std::tuple<vk::SwapchainKHR, vk::Extent2D> createSwapchain(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        vk::SurfaceKHR surface,
        uint32_t graphicsIndex,
        uint32_t presentIndex,
        SDL_Window* window,
        vk::SurfaceFormatKHR fmt,
        vk::SwapchainKHR old = nullptr
);

static void createSwapchainImages(
        std::vector<vk::Image>& images,
        std::vector<vk::ImageView>& views,
        vk::Device device,
        vk::SwapchainKHR swapchain,
        vk::SurfaceFormatKHR fmt
);

/// \brief Creates the logical vulkan device handle
/// \param physicalDevice The physical device to create the logical device for
/// \param extensions Vector of device extensions to enable
/// \param queueFamilies The QueueFamilies struct containing the queue families to enable for this device
/// \return The handle to the created device
static vk::Device createDevice(
        vk::PhysicalDevice physicalDevice,
        const std::vector<const char*>& extensions,
        const QueueFamilies& queueFamilies
);

static vk::SurfaceFormatKHR getSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);

dragonfire::rendering::RenderingEngine::RenderingEngine(SDL_Window* window, bool validation)
    : barrier(renderThreadCount + 1) {
    assert(window);
    // basic vulkan initialization
    instance = createInstance(window, validation ? RenderingEngine::debugCallback : nullptr);
    surface = createSurface(window, instance);
    assert(surface);

    std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    };
    physicalDevice = getPhysicalDevice(instance, surface, deviceExtensions);
    spdlog::info("Using GPU {}", physicalDevice.getProperties().deviceName);
    QueueFamilies queueFamilies(physicalDevice, surface);
    device = createDevice(physicalDevice, deviceExtensions, queueFamilies);
    Allocation::initAllocator(instance, physicalDevice, device);

    graphicsQueue = device.getQueue(queueFamilies.graphicsIndex, 0);
    presentationQueue = device.getQueue(queueFamilies.presentationIndex, 0);
    surfaceFormat = getSurfaceFormat(physicalDevice, surface);

    auto [chain, extent] = createSwapchain(
            device,
            physicalDevice,
            surface,
            queueFamilies.graphicsIndex,
            queueFamilies.presentationIndex,
            window,
            surfaceFormat
    );
    swapchain = chain;
    swapchainExtent = extent;
    createSwapchainImages(images, views, device, swapchain, surfaceFormat);

    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = queueFamilies.graphicsIndex};
    utilityPool = device.createCommandPool(poolInfo);

    // init per frame data
    for (auto& frame : frames) {
        vk::CommandPoolCreateInfo createInfo{
                .queueFamilyIndex = queueFamilies.graphicsIndex,
        };
        auto primaryPool = device.createCommandPool(createInfo);
        vk::CommandBufferAllocateInfo allocInfo{
                .commandPool = primaryPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
        };
        std::vector<vk::CommandPool> pools(renderThreadCount);
        for (auto& pool : pools)
            pool = device.createCommandPool(createInfo);
        std::vector<vk::CommandBuffer> buffers(renderThreadCount);
        for (int i = 0; i < renderThreadCount; i++) {
            vk::CommandBufferAllocateInfo secondaryAlloc{
                    .commandPool = pools[i],
                    .level = vk::CommandBufferLevel::eSecondary,
                    .commandBufferCount = 1,
            };

            buffers[i] = device.allocateCommandBuffers(secondaryAlloc)[0];
        }
        frame = Frame{
                .cmd = device.allocateCommandBuffers(allocInfo)[0],
                .primaryPool = primaryPool,
                .secondaryBuffers = std::move(buffers),
                .secondaryPools = std::move(pools),
                .ubo = GPUObject<UBO>(vk::BufferUsageFlagBits::eUniformBuffer),
                .fence = device.createFence(vk::FenceCreateInfo()),
                .presentSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo()),
                .renderSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo()),
        };
    }

    // init render threads
    renderThreads.reserve(renderThreadCount);
    threadQueues.reserve(renderThreadCount);
    for (auto i = 0; i < renderThreadCount; i++) {
        threadQueues.emplace_back(THREAD_BUFFER_CAPACITY);
        renderThreads.emplace_back(std::bind_front(&dragonfire::rendering::RenderingEngine::renderThread, this), i);
    }
    presentationThread = std::jthread(std::bind_front(&dragonfire::rendering::RenderingEngine::present, this));

    spdlog::info("Rendering engine initialization finished");
}

static void createSwapchainImages(
        std::vector<vk::Image>& images,
        std::vector<vk::ImageView>& views,
        vk::Device device,
        vk::SwapchainKHR swapchain,
        vk::SurfaceFormatKHR fmt
) {
    images = device.getSwapchainImagesKHR(swapchain);
    views.reserve(images.size());
    for (auto& image : images) {
        vk::ImageSubresourceRange subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
        };
        vk::ImageViewCreateInfo createInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = fmt.format,
                .components =
                        vk::ComponentMapping{
                                .r = vk::ComponentSwizzle::eIdentity,
                                .g = vk::ComponentSwizzle::eIdentity,
                                .b = vk::ComponentSwizzle::eIdentity,
                                .a = vk::ComponentSwizzle::eIdentity,
                        },
                .subresourceRange = subresourceRange,
        };
        views.push_back(device.createImageView(createInfo));
    }
}

static vk::PresentModeKHR getPresentMode(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) {
    auto modes = physicalDevice.getSurfacePresentModesKHR(surface);
    for (const auto mode : modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            spdlog::info("Using mailbox presentation mode");
            return mode;
        }
    }
    spdlog::info("Using FIFO presentation mode");
    return vk::PresentModeKHR::eFifo;
}

static vk::SurfaceFormatKHR getSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) {
    uint32_t count;
    vk::SurfaceFormatKHR* formats = nullptr;
    vk::resultCheck(
            physicalDevice.getSurfaceFormatsKHR(surface, &count, formats),
            "Failed to get surface format count"
    );
    if (count == 0)
        throw std::runtime_error("No surface formats available");
    formats = (vk::SurfaceFormatKHR*) alloca(sizeof(vk::SurfaceFormatKHR) * count);
    vk::resultCheck(physicalDevice.getSurfaceFormatsKHR(surface, &count, formats), "Failed to get surface formats");

    for (uint32_t i = 0; i < count; i++) {
        auto& fmt = formats[i];
        if (fmt.format == vk::Format::eB8G8R8Srgb && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return fmt;
    }
    spdlog::warn(
            "Preferred surface format not found, using format: {}, color space: {} instead",
            to_string(formats[0].format),
            to_string(formats[0].colorSpace)
    );
    return formats[0];
}

static std::tuple<vk::SwapchainKHR, vk::Extent2D> createSwapchain(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        vk::SurfaceKHR surface,
        uint32_t graphicsIndex,
        uint32_t presentIndex,
        SDL_Window* window,
        vk::SurfaceFormatKHR fmt,
        vk::SwapchainKHR old
) {
    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    vk::Extent2D extent;
    if (capabilities.currentExtent.width == UINT32_MAX) {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        extent.width = width;
        extent.height = height;
    }
    else
        extent = capabilities.currentExtent;

    uint32_t imageCount = capabilities.maxImageCount == 0
                                  ? capabilities.minImageCount + 1
                                  : std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

    vk::SharingMode sharingMode =
            graphicsIndex == presentIndex ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

    uint32_t indices[] = {graphicsIndex, presentIndex};
    vk::SwapchainCreateInfoKHR createInfo{
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = fmt.format,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = sharingMode,
            .queueFamilyIndexCount = 2,
            .pQueueFamilyIndices = indices,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = getPresentMode(physicalDevice, surface),
            .oldSwapchain = old,
    };

    return std::make_tuple(device.createSwapchainKHR(createInfo), extent);
}

QueueFamilies::QueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    vk::QueueFamilyProperties* props = nullptr;
    uint32_t size;
    device.getQueueFamilyProperties(&size, props);
    props = (vk::QueueFamilyProperties*) alloca(sizeof(vk::QueueFamilyProperties) * size);
    device.getQueueFamilyProperties(&size, props);

    bool foundGraphics = false, foundPresent = false;
    for (auto i = 0; i < size; i++) {
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
    // auto props = device.getProperties();
    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures;
    vk::PhysicalDeviceFeatures2 features;
    features.pNext = &dynamicRenderingFeatures;
    device.getFeatures2(&features);
    if (!dynamicRenderingFeatures.dynamicRendering)
        return false;

    // check if device supports all requested extensions
    vk::ExtensionProperties* extProps = nullptr;
    uint32_t size;
    vk::resultCheck(
            device.enumerateDeviceExtensionProperties(nullptr, &size, extProps),
            "Failed to get device extensions size"
    );
    extProps = (vk::ExtensionProperties*) alloca(sizeof(vk::ExtensionProperties) * size);
    vk::resultCheck(
            device.enumerateDeviceExtensionProperties(nullptr, &size, extProps),
            "Failed to get device extensions"
    );

    for (const auto& ext : requestedExtensions) {
        bool found = false;
        for (uint32_t i = 0; i < size; i++) {
            if (strcmp(extProps[i].extensionName, ext) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    // Check if the device has the required queue families
    try {
        QueueFamilies(device, surface);
    }
    catch (const std::runtime_error& e) {
        return false;
    }

    //todo check other stuff
    return true;
}

static vk::PhysicalDevice getPhysicalDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        const std::vector<const char*>& deviceExtensions
) {
    vk::PhysicalDevice* devices = nullptr;
    uint32_t size;
    vk::resultCheck(instance.enumeratePhysicalDevices(&size, devices), "Failed to get device count");
    devices = (vk::PhysicalDevice*) alloca(sizeof(vk::PhysicalDevice) * size);
    vk::resultCheck(instance.enumeratePhysicalDevices(&size, devices), "Failed to get devices");

    if (size == 0)
        throw std::runtime_error("No GPU found");

    for (const char* ext : deviceExtensions)
        spdlog::info("Loading device extension {}", ext);

    vk::PhysicalDevice valid = nullptr;
    for (uint32_t i = 0; i < size; i++) {
        if (isValidDevice(devices[i], surface, deviceExtensions)) {
            valid = devices[i];
            if (devices[i].getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                return devices[i];
        }
    }

    if (valid) {
        spdlog::warn("No discrete GPU found, falling back to integrated gpu");
        return valid;
    }

    throw std::runtime_error("No suitable GPU available");
}

static std::vector<const char*> getInstanceExtensions(SDL_Window* window, bool validation) {
    unsigned int count;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
    std::vector<const char*> extensions(count);
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
                               | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
                               | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
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
    for (const auto& ext : extensions)
        spdlog::info("Loaded instance extension: {}", ext);
    auto layers = getLayers(debugCallback != nullptr);
    for (const auto& layer : layers)
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
            "Vulkan version {:d}.{:d}.{:d} loaded",
            VK_API_VERSION_MAJOR(vkVersion),
            VK_API_VERSION_MINOR(vkVersion),
            VK_API_VERSION_PATCH(vkVersion)
    );

    return instance;
}