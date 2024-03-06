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

    ~App() override;

protected:
    void mainLoop(double deltaTime) override;

private:
    Model model;
    Camera camera;
    std::unique_ptr<BaseRenderer> renderer;
};

}// namespace dragonfire