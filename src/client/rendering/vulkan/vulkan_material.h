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

    static void initDefaultMaterial(vk::SampleCountFlagBits sampleCount, PipelineFactory& pipelineFactory);
};

inline void VulkanMaterial::initDefaultMaterial(
    const vk::SampleCountFlagBits sampleCount,
    PipelineFactory& pipelineFactory
)
{
    constexpr TextureIds ids{};
    PipelineInfo pipelineInfo{};
    pipelineInfo.enableColorBlend = false;
    pipelineInfo.sampleCount = sampleCount;
    pipelineInfo.enableMultisampling = sampleCount != vk::SampleCountFlagBits::e1;
    pipelineInfo.topology = vk::PrimitiveTopology::eTriangleList;

    pipelineInfo.rasterState.frontFace = vk::FrontFace::eCounterClockwise;
    pipelineInfo.rasterState.cullMode = vk::CullModeFlagBits::eBack;
    pipelineInfo.rasterState.polygonMode = vk::PolygonMode::eFill;
    pipelineInfo.rasterState.lineWidth = 1.0f;

    pipelineInfo.depthState.depthWriteEnable = pipelineInfo.depthState.depthTestEnable = true;
    pipelineInfo.depthState.depthCompareOp = vk::CompareOp::eLessOrEqual;
    pipelineInfo.depthState.depthBoundsTestEnable = false;
    pipelineInfo.depthState.stencilTestEnable = false;
    pipelineInfo.depthState.minDepthBounds = 0.0f;
    pipelineInfo.depthState.maxDepthBounds = 1.0f;
    pipelineInfo.vertexCompShader = "default.vert";
    pipelineInfo.fragmentShader = "default.frag";
    const Pipeline pipeline = pipelineFactory.getOrCreate(pipelineInfo);
    const auto mat = std::make_shared<VulkanMaterial>(ids, pipeline);
    Material::DEFAULT = mat;
}

}// namespace dragonfire::vulkan
