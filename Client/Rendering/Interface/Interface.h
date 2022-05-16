//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <SDL.h>
#include <Service.h>

namespace dragonfire::rendering {
    struct IRenderEngine : public Service {

    };
    extern void initRendering(SDL_Window* window);
    extern const SDL_WindowFlags RequiredFlags;
}