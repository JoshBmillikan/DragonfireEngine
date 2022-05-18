//
// Created by Josh on 5/14/2022.
//
#include "RenderingEngine.h"
#include <Interface.h>

#ifndef NDEBUG
    #define VALIDATION true
#else
    #define VALIDATION false
#endif

namespace dragonfire::rendering {
void initRendering(SDL_Window* window) { Service::init<RenderingEngine>(window, VALIDATION); }
const SDL_WindowFlags RequiredFlags = SDL_WINDOW_VULKAN;
}   // namespace dragonfire::rendering