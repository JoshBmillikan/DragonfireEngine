//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <Engine.h>

namespace dragonfire {

class EXPORTED Game {
protected:
    bool running = true;
    virtual void mainLoop(double deltaTime) = 0;
public:
    Game(int argc, char** argv, spdlog::level::level_enum level = spdlog::level::info);

    void run();

    virtual ~Game() noexcept;
};

}   // namespace dragonfire
