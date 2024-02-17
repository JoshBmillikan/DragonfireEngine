//
// Created by josh on 1/15/24.
//

#include "base_renderer.h"

#include "core/config.h"
#include "core/crash.h"
#include "model.h"
#include <spdlog/spdlog.h>

namespace dragonfire {
BaseRenderer::BaseRenderer()
{
    logger = spdlog::get("Rendering");
    if (!logger) {
        logger = spdlog::default_logger()->clone("Rendering");
        register_logger(logger);
    }
}

BaseRenderer::BaseRenderer(int windowFlags) : BaseRenderer()
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

    window = SDL_CreateWindow(
        APP_DISPLAY_NAME,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        windowFlags
    );
    if (window == nullptr)
        crash("SDL failed to create window: {}", SDL_GetError());
    logger->info("Created window of size {}x{}", width, height);
}

BaseRenderer::~BaseRenderer() noexcept
{
    SDL_DestroyWindow(window);
}

void BaseRenderer::addDrawable(const Model* model, const Transform& transform)
{
    for (size_t i = 0; i < model->primitiveCount(); i++) {
        const Model::Primitive& primitive = (*model)[i];
        glm::mat4 base = transform.toMatrix();
        drawables[primitive.material]
            .emplace_back(primitive.mesh, base * primitive.transform, primitive.bounds);
    }
}

void BaseRenderer::addDrawables(const Model* model, const std::span<Transform> transforms)
{
    for (auto& t : transforms)
        addDrawable(model, t);
}

void BaseRenderer::render(const Camera& camera)
{
    beginFrame(camera);
    drawModels(camera, drawables);
    endFrame();
}

void BaseRenderer::endFrame()
{
    drawables.clear();
    frameCount++;
}

}// namespace dragonfire