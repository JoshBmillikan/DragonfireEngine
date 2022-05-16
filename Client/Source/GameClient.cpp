//
// Created by Josh on 5/14/2022.
//

#include "GameClient.h"
#include "Settings.h"

namespace dragonfire {

static SDL_Window* createWindow() {
    uint32_t flags = rendering::RequiredFlags;
    auto& settings = Service::get<Settings>();
    auto windowMode = settings.get<std::string>("graphics.window.mode");
    auto& resolution = settings.getVec<int64_t>("graphics.window.resolution");

    if (windowMode == "fullscreen")
        flags |= SDL_WINDOW_FULLSCREEN;
    else if (windowMode == "borderless")
        flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED;
    else if (windowMode == "windowed")
        flags |= SDL_WINDOW_RESIZABLE;
    else {
        spdlog::error("Unknown window mode {}, defaulting to windowed", windowMode);
        flags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_Window* window = SDL_CreateWindow(
            APP_NAME,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            (int)resolution[0],
            (int)resolution[1],
            flags);

    if (window)
        return window;

    throw std::runtime_error("SDL failed to create window");
}

GameClient::GameClient(int argc, char** argv) : Game(argc, argv), window(createWindow(), &SDL_DestroyWindow) {
    rendering::initRendering(window.get());
}

void GameClient::mainLoop(double deltaTime) {
    SDL_Event event;
    auto& renderingEngine = Service::get<rendering::IRenderEngine>();
    while (SDL_PollEvent(&event)) {
        //todo events
    }
    // todo dispatch world updates
    // todo render world
}

}   // namespace dragonfire