//
// Created by josh on 1/26/24.
//

#pragma once
#include <cstdint>

namespace dragonfire {
struct alignas(16) TextureIds {
    uint32_t albedo = 0;
    uint32_t normal = 0;
    uint32_t ambient = 0;
    uint32_t diffuse = 0;
    uint32_t specular = 0;
};

class Material {
    TextureIds textureIds;
    uint32_t pipelineId = 0;

public:
    Material(const TextureIds& textureIds, const uint32_t pipelineId)
        : textureIds(textureIds), pipelineId(pipelineId)
    {
    }

    virtual void foo() = 0;

    virtual ~Material() = default;

    [[nodiscard]] const TextureIds& getTextures() const { return textureIds; }

    [[nodiscard]] uint32_t getPipelineId() const { return pipelineId; }
};
}// namespace dragonfire
