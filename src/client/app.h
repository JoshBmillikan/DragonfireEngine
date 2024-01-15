//
// Created by josh on 1/13/24.
//

#pragma once
#include <core/engine.h>

namespace dragonfire {

class App final : public Engine {
public:
    App(int argc, char** argv);

    ~App() override;

protected:
    void mainLoop(double deltaTime) override;
    cxxopts::OptionAdder getCommandLineOptions(cxxopts::OptionAdder& options) override;
};

} // raven