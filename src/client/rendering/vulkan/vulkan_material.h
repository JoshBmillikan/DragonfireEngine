//
// Created by josh on 2/25/24.
//

#pragma once
#include "pipeline.h"
#include <client/rendering/material.h>

namespace dragonfire::vulkan {

struct VulkanMaterial final : Material {
    Pipeline pipeline;

    VulkanMaterial(const TextureIds& textureIds, const Pipeline& pipeline)
        : Material(textureIds, pipeline.getId()), pipeline(pipeline)
    {
    }

    ~VulkanMaterial() override = default;
    void foo() override;
};

}// namespace dragonfire::vulkan
