//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <SDL.h>

namespace dragonfire::rendering {
    struct IRenderEngine {

        virtual ~IRenderEngine() = default;
    };
    extern IRenderEngine* initRendering(SDL_Window* window);
    extern const SDL_WindowFlags RequiredFlags;
}