//
// Created by josh on 1/13/24.
//

#include "app.h"
#include "core/config.h"
#include "rendering/vulkan/vulkan_renderer.h"
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
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

App::App(const int argc, char** const argv) : Engine(false, argc, argv, extraCommands)
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
    auto model = loader->load("assets/models/bunny.glb");
    loader.reset();
    SDL_Window* window = renderer->getWindow();
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    camera = Camera(45.0f, float(w), float(h), 0.1f, 1000.0f);

    world = std::make_unique<GameWorld>();
    Transform t = glm::vec3();
    t.scale *= 4.0f;
    t.rotation = glm::rotate(t.rotation, glm::vec3(0.0f, glm::radians(180.0f), 0.0f));
    camera.position = glm::vec3(0.0f, 5.0f, 3.0f);
    const auto& ecs = world->getECSWorld();
    ecs.entity().set([&](Model& m, Transform& transform) {
        m = std::move(model);
        transform = t;
    });
    ecs.system<const Model, const Transform>()
        .kind(flecs::PostUpdate)
        .each([&](const Model& m, const Transform& transform) { renderer->addDrawable(&m, transform); });

    ecs.system<Transform>().each([](const flecs::iter& it, size_t, Transform& tr) {
        tr.rotation = glm::rotate(tr.rotation, it.delta_time() * 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    });
}

App::~App()
{
    world.reset();
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
    renderer->beginImGuiFrame();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                spdlog::info("Window closed");
                stop();
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE: camera.position.z += 3.0f * deltaTime; break;
                    case SDLK_LCTRL: camera.position.z -= 3.0f * deltaTime; break;
                    default: break;
                }
                break;
        }
    }
    ImGui::Begin("Frame Info");
    ImGui::Text("Frame time: %.1fms (%.1f FPS)", deltaTime * 1000, ImGui::GetIO().Framerate);
    static bool vsync = Config::get().getBool("vsync").value_or(true);
    const bool last = vsync;
    ImGui::Checkbox("Vsync", &vsync);
    if (last != vsync)
        renderer->setVsync(vsync);
    ImGui::End();
    if (!world->getECSWorld().progress(static_cast<float>(deltaTime)))
        stop();
    renderer->render(camera);
}

}// namespace dragonfire