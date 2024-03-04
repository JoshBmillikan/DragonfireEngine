//
// Created by josh on 1/13/24.
//

#include "app.h"

#include "core/config.h"
#include "rendering/vulkan/vulkan_renderer.h"

#include <SDL2/SDL.h>
#include <physfs.h>
#include <spdlog/spdlog.h>

namespace dragonfire {

static cxxopts::OptionAdder extraCommands(cxxopts::OptionAdder&& adder)
{
    return adder(
        "v,vulkan-validation",
        "Enable vulkan validation layers",
        cxxopts::value<bool>()->default_value("false")
    );
}

App::App(const int argc, char** const argv) : Engine(argc, argv, extraCommands)
{
    try {
        Config::get().loadJsonFile("config/config.json");
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to load config file: {}", e.what());
    }
    char** ptr = PHYSFS_enumerateFiles("assets/models");
    for (char** p = ptr; *p; p++)
        spdlog::warn("PATH: {}", *p);
    PHYSFS_freeList(ptr);

    renderer = std::make_unique<vulkan::VulkanRenderer>(cli["vulkan-validation"].as<bool>());
    auto loader = renderer->getModelLoader();
    Model bunny = loader->load("assets/models/bunny.glb");
    loader.reset();
}

App::~App()
{
    renderer.reset();
    try {
        PHYSFS_mkdir("config");
        Config::get().saveJson("config/config.json");
        spdlog::info("Saved config file to \"config/config.json\"");
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to save config to file: {}", e.what());
    }
}

void App::mainLoop(const double deltaTime)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                spdlog::info("Window closed");
                stop();
                break;
        }
    }
}

}// namespace dragonfire