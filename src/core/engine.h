//
// Created by josh on 1/13/24.
//

#pragma once
#include "world/game_world.h"
#include <sol/sol.hpp>
#include <cxxopts.hpp>
#include <functional>

namespace dragonfire {

class Engine {
public:
    Engine(
        bool remote,
        int argc,
        char** argv,
        std::function<cxxopts::OptionAdder(cxxopts::OptionAdder&&)>&& extraCli
        = [](cxxopts::OptionAdder&& it) { return it; }
    );
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
    virtual void mainLoop(double deltaTime) = 0;
    cxxopts::ParseResult cli;
    std::unique_ptr<GameWorld> world;

private:
    static Engine* INSTANCE;
    bool running = false;

    void parseCommandLine(
        int argc,
        char** argv,
        std::function<cxxopts::OptionAdder(cxxopts::OptionAdder&&)>&& extraCli
    );
};

}// namespace dragonfire