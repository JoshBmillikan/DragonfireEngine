//
// Created by josh on 1/15/24.
//

#pragma once
#include "camera.h"
#include "core/utility/temp_containers.h"
#include "core/world/transform.h"
#include "material.h"
#include "model.h"
#include <SDL_video.h>
#include <ankerl/unordered_dense.h>
#include <spdlog/logger.h>

namespace dragonfire {

class BaseRenderer {
public:
    BaseRenderer();
    explicit BaseRenderer(int windowFlags);

    explicit BaseRenderer(SDL_Window* window) : BaseRenderer() { this->window = window; }

    [[nodiscard]] SDL_Window* getWindow() const { return window; }

    virtual ~BaseRenderer() noexcept;
    virtual std::unique_ptr<Model::Loader> getModelLoader() = 0;

    void addDrawable(const Model* model, const Transform& transform);
    void addDrawables(const Model* model, std::span<Transform> transforms);
    void render(const Camera& camera);
    [[nodiscard]] uint64_t getFrameCount() const {return frameCount;}

protected:
    struct Draw {
        Mesh mesh;
        glm::mat4 transform;
        glm::vec4 bounds;
    };
    using Drawables = ankerl::unordered_dense::map<const Material*, TempVec<Draw>>;
    std::shared_ptr<spdlog::logger> logger;
    virtual void beginFrame(const Camera& camera) = 0;
    virtual void drawModels(const Camera& camera, const Drawables& models) = 0;
    virtual void endFrame();

private:
    SDL_Window* window = nullptr;
    uint64_t frameCount = 0;
    Drawables drawables;
};

}// namespace dragonfire