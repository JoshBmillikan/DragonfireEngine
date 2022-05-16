//
// Created by Josh on 5/14/2022.
//

#include "GameClient.h"

namespace dragonfire {


static SDL_Window* createWindow() {
    uint32_t flags = rendering::RequiredFlags;
//    auto& settings = Settings::getInstance<Dragonfire::Settings>();
//    auto resolution = settings.get<int[2]>("engine.graphics.resolution");
//    auto windowMode = settings.get<std::string>("engine.graphics.window_mode");
//    if (windowMode == "fullscreen")
//        flags |= SDL_WINDOW_FULLSCREEN;
//    else if (windowMode == "borderless")
//        flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED;
//    else if (windowMode == "windowed")
//        flags |= SDL_WINDOW_RESIZABLE;
//    else {
//        spdlog::error("Unknown window mode {}, defaulting to windowed", windowMode);
//        flags |= SDL_WINDOW_RESIZABLE;
//    }
//    SDL_Window* window = SDL_CreateWindow(
//            APP_NAME,
//            SDL_WINDOWPOS_CENTERED,
//            SDL_WINDOWPOS_CENTERED,
//            resolution[0],
//            resolution[1],
//            flags);
//    if (window)
//        return window;
//    throw std::runtime_error("SDL failed to create window");
    return nullptr;
}

GameClient::GameClient(int argc, char** argv) : Game(argc, argv), window(createWindow(),&SDL_DestroyWindow) {
    renderingEngine = std::unique_ptr<rendering::IRenderEngine>(rendering::initRendering(nullptr));
}

void GameClient::mainLoop(double deltaTime) {

}

}   // namespace dragonfire