//
// Created by josh on 1/13/24.
//

#pragma once
#include <cxxopts.hpp>
#include <functional>

namespace dragonfire {

class Engine {
public:
    Engine(
        int argc,
        char** argv,
        std::function<cxxopts::OptionAdder(cxxopts::OptionAdder&&)>&& extraCli
        = [](cxxopts::OptionAdder&& it) { return it; }
    );
    virtual ~Engine();
    void run();

    void stop() { running = false; }

protected:
    virtual void mainLoop(double deltaTime) = 0;
    cxxopts::ParseResult cli;

private:
    bool running = false;

    void parseCommandLine(
        int argc,
        char** argv,
        std::function<cxxopts::OptionAdder(cxxopts::OptionAdder&&)>&& extraCli
    );
};

}// namespace dragonfire