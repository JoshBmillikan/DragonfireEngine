//
// Created by josh on 1/15/24.
//

#pragma once
#include "core/world/transform.h"
#include <SDL_video.h>
#include <spdlog/logger.h>

namespace dragonfire {
class Model;

class BaseRenderer {
public:
    BaseRenderer();
    explicit BaseRenderer(int windowFlags);

    explicit BaseRenderer(SDL_Window* window) : BaseRenderer() { this->window = window; }

    [[nodiscard]] SDL_Window* getWindow() const { return window; }

    virtual ~BaseRenderer() noexcept;

protected:
    std::shared_ptr<spdlog::logger> logger;

private:
    SDL_Window* window = nullptr;
};

}// namespace dragonfire