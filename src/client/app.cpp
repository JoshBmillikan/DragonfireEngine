//
// Created by josh on 1/13/24.
//

#include "app.h"

namespace dragonfire {
App::App(const int argc, char** const argv) : Engine(argc, argv) {}

App::~App() {}

void App::mainLoop(const double deltaTime) {}

cxxopts::OptionAdder App::getCommandLineOptions(cxxopts::OptionAdder& options)
{
    return Engine::getCommandLineOptions(options);
}
}// namespace raven