//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "BaseMesh.h"
#include "IMaterial.h"
#include <SDL_video.h>
#include <Service.h>

namespace dragonfire::rendering {
struct IRenderEngine : public Service {
    virtual void beginRendering(const glm::mat4& view, const glm::mat4& projection) noexcept = 0;
    virtual void render(BaseMesh* meshPtr, IMaterial* materialPtr, const glm::mat4& transform) noexcept = 0;
    virtual void endRendering() noexcept = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};
void initRendering(SDL_Window* window);
extern const SDL_WindowFlags RequiredFlags;
}   // namespace dragonfire::rendering