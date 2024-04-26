//
// Created by josh on 1/13/24.
//

#pragma once
#include <client/rendering/base_renderer.h>
#include <core/engine.h>
#include <memory>

namespace dragonfire {

class App final : public Engine {
public:
    App(int argc, char** argv);
    void init() override;

    ~App() override;

protected:
    void mainLoop(double deltaTime) override;
    cxxopts::OptionAdder getExtraCliOptions(cxxopts::OptionAdder&& options) override;

private:
    std::unique_ptr<BaseRenderer> renderer;
};

}// namespace dragonfire