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

App::App(const int argc, char** const argv) : Engine(false, argc, argv) {}

void App::init()
{
    Engine::init();
    try {
        Config::get().loadJsonFile("config/config.json");
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to load config file: {}", e.what());
    }

    const bool vsync = cli["vulkan-validation"].as<bool>();
    renderer = std::unique_ptr<BaseRenderer>(BaseRenderer::createRenderer(vsync));
    const auto loader = renderer->getModelLoader();
    assetManager.loadDirectory("assets/models", loader.get());
    auto [w, h] = renderer->getWindowSize();
    auto camera = Camera(45.0f, float(w), float(h), 0.1f, 1000.0f);

    world = std::make_unique<GameWorld>();
    Transform t = glm::vec3();
    t.rotation = glm::rotate(t.rotation, glm::vec3(0.0f, glm::radians(180.0f), 0.0f));
    camera.position = glm::vec3(0.0f, 5.0f, 3.0f);
    const auto& ecs = world->getECSWorld();
    ecs.singleton<Camera>().set(camera);
    for (uint32_t i = 0; i < 10; i++) {
        ecs.entity().set([&](AssetRef<Model>& m, Transform& transform) {
            m = assetManager.get<Model>("stanford-bunny");
            t.position.x += float(i) * 5.0f;
            transform = t;
        });
    }

    struct StaticObject {};

    ecs.entity()
        .set([&](AssetRef<Model>& m, Transform& transform) {
            m = assetManager.get<Model>("Cube");
            transform = Transform();
        })
        .add<StaticObject>();
    ecs.system<const AssetRef<Model>, const Transform>()
        .kind(flecs::PostUpdate)
        .each([&](const AssetRef<Model>& m, const Transform& transform) {
            renderer->addDrawable(m, transform);
        });

    ecs.system<Transform>().without<StaticObject>().each([](const flecs::iter& it, size_t, Transform& tr) {
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
    assetManager.clear();
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
                    case SDLK_SPACE:
                        world->getECSWorld().singleton<Camera>().get_mut<Camera>()->position.z
                            += 3.0f * float(deltaTime);
                        break;
                    case SDLK_LCTRL:
                        world->getECSWorld().singleton<Camera>().get_mut<Camera>()->position.z
                            -= 3.0f * float(deltaTime);
                        break;
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

cxxopts::OptionAdder App::getExtraCliOptions(cxxopts::OptionAdder&& options)
{
    return options(
        "v,vulkan-validation",
        "Enable vulkan validation layers",
        cxxopts::value<bool>()->default_value("false")
    );
}

}// namespace dragonfire