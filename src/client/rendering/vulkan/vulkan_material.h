//
// Created by josh on 2/25/24.
//

#pragma once
#include "pipeline.h"
#include <client/rendering/material.h>

namespace dragonfire::vulkan {

class VulkanMaterial final : public Material {
public:
    Pipeline pipeline;

    VulkanMaterial(const TextureIds& textureIds, const Pipeline& pipeline)
        : Material(textureIds, pipeline.getId()), pipeline(pipeline)
    {
    }

    void foo() override
    {
        // TODO REMOVE ME
    }
};

}// namespace dragonfire::vulkan
