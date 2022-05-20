//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <Game.h>
#include <SDL.h>

namespace dragonfire {

class GameClient : public Game {
    std::unique_ptr<SDL_Window, void (*)(SDL_Window*)> window;

public:
    GameClient(int argc, char** argv);

protected:
    void mainLoop(double deltaTime) override;
};

}   // namespace dragonfire
