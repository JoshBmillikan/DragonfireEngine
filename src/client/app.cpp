//
// Created by josh on 1/13/24.
//

#include "app.h"
#include "core/config.h"
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

    renderer
        = std::unique_ptr<BaseRenderer>(BaseRenderer::createRenderer(cli["vulkan-validation"].as<bool>()));
    auto loader = renderer->getModelLoader();
    auto model = loader->load("assets/models/bunny.glb");
    auto floor = loader->load("assets/models/floor.glb");
    loader.reset();
    SDL_Window* window = renderer->getWindow();
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    auto camera = Camera(45.0f, float(w), float(h), 0.1f, 1000.0f);

    world = std::make_unique<GameWorld>();
    Transform t = glm::vec3();
    t.scale *= 4.0f;
    t.rotation = glm::rotate(t.rotation, glm::vec3(0.0f, glm::radians(180.0f), 0.0f));
    camera.position = glm::vec3(0.0f, 5.0f, 3.0f);
    const auto& ecs = world->getECSWorld();
    ecs.singleton<Camera>().set(camera);
    ecs.entity().set([&](Model& m, Transform& transform) {
        m = std::move(model);
        transform = t;
    });
    ecs.entity().set([&](Model& m, Transform& transform) {
        m = std::move(floor);
        transform = Transform();
    });
    ecs.system<const Model, const Transform>()
        .kind(flecs::PostUpdate)
        .each([&](const Model& m, const Transform& transform) { renderer->addDrawable(&m, transform); });

    ecs.system<Transform>().each([](const flecs::iter& it, size_t, Transform& tr) {
        tr.rotation = glm::slerp(
            tr.rotation,
            glm::rotate(tr.rotation, 2.0f, glm::vec3(0.0f, 0.0f, 1.0f)),
            it.delta_time()
        );
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
                    case SDLK_SPACE: world->getECSWorld().singleton<Camera>().get_mut<Camera>()->position.z += 3.0f * deltaTime; break;
                    case SDLK_LCTRL: world->getECSWorld().singleton<Camera>().get_mut<Camera>()->position.z -= 3.0f * deltaTime; break;
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
    if (!world->getECSWorld().progress(static_cast<float>(deltaTime)))
        stop();
    ImGui::Text("Model count: %d", renderer->getDrawCount());
    ImGui::End();
    renderer->render(*world->getECSWorld().singleton<Camera>().get<Camera>());
}

}// namespace dragonfire