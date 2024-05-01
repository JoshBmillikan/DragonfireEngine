//
// Created by josh on 1/15/24.
//

#include "base_renderer.h"
#include "core/config.h"
#include "core/crash.h"
#include "model.h"
#include "vulkan/vulkan_renderer.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <spdlog/spdlog.h>

namespace dragonfire {

static SDL_Window* initWindow(int windowFlags, spdlog::logger* logger);
static void initImGui();

BaseRenderer::BaseRenderer()
{
    logger = spdlog::get("Rendering");
    if (!logger) {
        logger = spdlog::default_logger()->clone("Rendering");
        register_logger(logger);
    }
}

BaseRenderer::BaseRenderer(const int windowFlags, void (*imguiRenderNewFrameCallback)()) : BaseRenderer()
{
    this->imguiRenderNewFrameCallback = imguiRenderNewFrameCallback;

    window = initWindow(windowFlags, logger.get());
    if (window == nullptr)
        crash("Failed to create window: {}", SDL_GetError());
    initImGui();
}

BaseRenderer::BaseRenderer(SDL_Window* window, void (*imguiRenderNewFrameCallback)()) : BaseRenderer()
{
    this->window = window;
    this->imguiRenderNewFrameCallback = imguiRenderNewFrameCallback;
    initImGui();
}

SDL_Window* initWindow(int windowFlags, spdlog::logger* logger)
{
    const auto& cfg = Config::get();
    switch (const int64_t mode = cfg.getInt("windowMode").value_or(INT64_MAX)) {
        case 1: windowFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MOUSE_CAPTURE; break;
        case 2:
            windowFlags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MOUSE_CAPTURE;
        break;
        default: SPDLOG_LOGGER_ERROR(logger, "Invalid window mode {}", mode);
        case INT64_MAX: SPDLOG_LOGGER_WARN(logger, "Window mode not set, running in windowed mode");
        case 0: windowFlags |= SDL_WINDOW_RESIZABLE; break;
    }

    int width, height;
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) < 0) {
        SPDLOG_LOGGER_ERROR(logger, "SDL failed to get display mode: {}", SDL_GetError());
        width = int(cfg.getInt("resolutionX").value_or(1920));
        height = int(cfg.getInt("resolutionY").value_or(1080));
    }
    else {
        width = std::min(int(cfg.getInt("resolutionX").value_or(dm.w / 2)), dm.w);
        height = std::min(int(cfg.getInt("resolutionY").value_or(dm.h / 2)), dm.h);
    }

    SDL_Window* window = SDL_CreateWindow(
        APP_DISPLAY_NAME,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        windowFlags
    );
    if (window)
        logger->info("Created window of size {}x{}", width, height);
    return window;
}

void initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    ImGui::StyleColorsDark();
}

BaseRenderer::~BaseRenderer() noexcept
{
    SDL_DestroyWindow(window);
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void BaseRenderer::addDrawable(const Drawable* drawable, const Transform& transform)
{
    drawable->writeDrawData(drawables, transform);
}

void BaseRenderer::addDrawables(const Drawable* drawable, const std::span<Transform> transforms)
{
    for (auto& t : transforms)
        addDrawable(drawable, t);
}

void BaseRenderer::render(const Camera& camera)
{
    ImGui::Render();
    beginFrame(camera);
    drawModels(camera, drawables);
    endFrame();
}

void BaseRenderer::setVsync(const bool vsync)
{
    Config::get().setVar("vsync", vsync);
}

void BaseRenderer::beginImGuiFrame() const
{
    if (imguiRenderNewFrameCallback)
        imguiRenderNewFrameCallback();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

BaseRenderer* BaseRenderer::createRenderer(const bool enableVsync)
{
    return new vulkan::VulkanRenderer(enableVsync);
}

void BaseRenderer::endFrame()
{
    drawables.clear();
    frameCount++;
    ImGui::EndFrame();
}

}// namespace dragonfire