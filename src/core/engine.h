//
// Created by josh on 1/13/24.
//

#pragma once
#include <cxxopts.hpp>

namespace dragonfire {

class Engine {
public:
    Engine(int argc, char** argv);
    virtual ~Engine();
    void run();

    void stop() { running = false; }

protected:
    virtual void mainLoop(double deltaTime) = 0;
    virtual cxxopts::OptionAdder getCommandLineOptions(cxxopts::OptionAdder& options);
    cxxopts::ParseResult cli;

private:
    bool running = false;

    void parseCommandLine(int argc, char** argv);
};

}// namespace raven