//
// Created by Josh on 5/14/2022.
//

#include "GameClient.h"
#include "Settings.h"
#include <Interface.h>

using namespace dragonfire;

static SDL_Window* createWindow();

void GameClient::mainLoop(double deltaTime) {
    SDL_Event event;
    auto& renderingEngine = Service::get<rendering::IRenderEngine>();
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        renderingEngine.resize(event.window.data1, event.window.data2);
                        break;
                }
                break;
        }
        //todo events
    }
    // todo dispatch world updates
    // todo render world
}

GameClient::GameClient(int argc, char** argv) : Game(argc, argv), window(createWindow(), &SDL_DestroyWindow) {
    rendering::initRendering(window.get());
}

static SDL_Window* createWindow() {
    uint32_t flags = rendering::RequiredFlags | SDL_WINDOW_ALLOW_HIGHDPI;
    auto& settings = Service::get<Settings>();
    auto windowMode = settings.get<std::string>("graphics.window.mode");
    auto& resolution = settings.getVec<int64_t>("graphics.window.resolution");

    spdlog::info("Creating window with {}x{} resolution in {} mode", resolution[0], resolution[1], windowMode);
    if (windowMode == "fullscreen")
        flags |= SDL_WINDOW_FULLSCREEN;
    else if (windowMode == "borderless")
        flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED;
    else {
        if (windowMode != "windowed")
            spdlog::error("Unknown window mode {}, defaulting to windowed", windowMode);
        // Disable bypassing the compositor when windowed, as it can cause bugs with kde
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
        flags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_Window* window = SDL_CreateWindow(
            APP_NAME,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            (int) resolution[0],
            (int) resolution[1],
            flags
    );

    if (window)
        return window;

    throw std::runtime_error("SDL failed to create window");
}
