//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "Material.h"
#include "Mesh.h"
#include "Swapchain.h"
#include <Interface.h>
#include <barrier>
#include <readerwritercircularbuffer.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace dragonfire::rendering {
class RenderingEngine final : public IRenderEngine {
public:
    /// \brief Constructs the rendering engine
    /// \param window the SDL window to render to
    /// \param validation if true, enable validation layers
    RenderingEngine(SDL_Window* window, bool validation);

    /// \brief Cleans up the rendering engine and joins all rendering threads
    ~RenderingEngine() override;

    /// \brief Start rendering the current frame
    /// \param view the camera view matrix
    /// \param projection the camera projection matrix
    void beginRendering(const glm::mat4& view, const glm::mat4& projection) noexcept override;

    /// \brief Renders a object in the world
    /// \param meshPtr a pointer to the mesh to render
    /// \param materialPtr a pointer to the material to render the mesh with
    /// \param transform the model transform matrix
    void render(BaseMesh* meshPtr, IMaterial* materialPtr, const glm::mat4& transform) noexcept override;

    /// End rendering the current frame and submit the commands to the GPU
    void endRendering() noexcept override;

    /// \brief Resizes all data dependant on the screen size, including the swapchain
    /// \param width the new width in pixels
    /// \param height the new height in pixels
    void resize(uint32_t width, uint32_t height) override;

    // No copy or move
    RenderingEngine(RenderingEngine&& other) = delete;
    RenderingEngine(RenderingEngine& other) = delete;
    RenderingEngine& operator=(RenderingEngine&& other) = delete;
    RenderingEngine& operator=(RenderingEngine& other) = delete;

public:
    constexpr static int FRAMES_IN_FLIGHT = 2;
    const uint32_t renderThreadCount = std::max(std::thread::hardware_concurrency() / 2, 1u);

private:
    /// \brief This function runs in a separate thread and records secondary command buffers
    /// \param token stop token to signal when the thread should stop
    /// \param index The threads index, to be used for access thread specific data in arrays
    void renderThread(const std::stop_token& token, uint32_t index) noexcept;

    /// \brief Uploads the current command buffers to the GPU and presents the image, runs in its own thread
    /// \param token stop token to signal when the thread should stop
    void present(const std::stop_token& token) noexcept;

    /// callback to log debug messages from the validation layers
    /// Never call this manually, pass it as a function pointer in the debug messenger create info struct
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
    ) noexcept;

private:
    constexpr static int THREAD_BUFFER_CAPACITY = 16;
    struct UBO {
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct Frame {
        vk::CommandBuffer cmd;
        vk::CommandPool primaryPool;
        std::vector<vk::CommandBuffer> secondaryBuffers;
        std::vector<vk::CommandPool> secondaryPools;
        GPUObject<UBO> ubo;
        vk::Fence fence;
        vk::Semaphore presentSemaphore, renderSemaphore;
    } frames[FRAMES_IN_FLIGHT];

    /// \brief Gets the current frame in flight
    /// \return A reference to the current frame
    inline Frame& getCurrentFrame() noexcept { return frames[frameCount % FRAMES_IN_FLIGHT]; }

    struct RenderCommand {
        enum class RenderState { Start, Render, End } state = RenderState::Start;
        Mesh* mesh = nullptr;
        Material* material = nullptr;
        const glm::mat4* transform = nullptr;
    };

    struct PresentData {
        Frame& frame;
        uint32_t imageIndex;
        PresentData(Frame& frame, uint32_t imageIndex) : frame(frame), imageIndex(imageIndex) {}
    };

private:
    std::atomic_uint64_t frameCount = 0;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;
    vk::Queue graphicsQueue, presentationQueue;
    Mesh* lastRenderedMesh = nullptr;
    Material* lastRenderedMaterial = nullptr;

    std::barrier<> barrier;
    std::vector<std::jthread> renderThreads;
    std::vector<moodycamel::BlockingReaderWriterCircularBuffer<RenderCommand>> threadQueues;

    std::jthread presentationThread;
    std::mutex presentMutex;
    std::condition_variable_any presentCond;
    std::optional<PresentData> presentData;

    Swapchain swapchain;
};
}   // namespace dragonfire::rendering