//
// Created by josh on 1/13/24.
//

#pragma once
#include "world/game_world.h"
#include <cxxopts.hpp>
#include <functional>
#include <sol/sol.hpp>

namespace dragonfire {

class Engine {
public:
    Engine(
        bool remote,
        int argc,
        char** argv
    );
    virtual void init();
    virtual ~Engine();
    void run();

    void stop() { running = false; }

    const bool remote;

    template<typename T>
        requires std::is_base_of_v<Engine, T>
    static T* getInstance() noexcept
    {
        return dynamic_cast<T*>(INSTANCE);
    }

    sol::state lua;

protected:
    int argc;
    char** argv;
    virtual void mainLoop(double deltaTime) = 0;

    virtual cxxopts::OptionAdder getExtraCliOptions(cxxopts::OptionAdder&& options) { return options; }

    cxxopts::ParseResult cli;
    std::unique_ptr<GameWorld> world;

private:
    static Engine* INSTANCE;
    bool running = false;

    void parseCommandLine();
};

}// namespace dragonfire