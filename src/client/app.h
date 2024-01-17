//
// Created by josh on 1/13/24.
//

#pragma once
#include <core/engine.h>
#include <memory>
#include <client/rendering/base_renderer.h>

namespace dragonfire {

class App final : public Engine {
public:
    App(int argc, char** argv);

    ~App() override;

protected:
    void mainLoop(double deltaTime) override;

private:
    std::unique_ptr<BaseRenderer> renderer;
};

} // raven