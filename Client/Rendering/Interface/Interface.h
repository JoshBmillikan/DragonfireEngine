//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <SDL.h>
#include <Service.h>

namespace dragonfire::rendering {
struct IRenderEngine : public Service {

    virtual void beginRendering() noexcept = 0;
    virtual void render() noexcept = 0;
    virtual void endRendering() noexcept = 0;
};
extern void initRendering(SDL_Window* window);
extern const SDL_WindowFlags RequiredFlags;
}   // namespace dragonfire::rendering