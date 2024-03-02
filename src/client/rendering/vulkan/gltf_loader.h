//
// Created by josh on 1/29/24.
//

#pragma once
#include "allocation.h"
#include "client/rendering/model.h"
#include "core/utility/small_vector.h"
#include "mesh.h"
#include "pipeline.h"
#include "staging_buffer.h"
#include "texture.h"
#include <fastgltf/parser.hpp>

namespace dragonfire::vulkan {

class VulkanGltfLoader final : public Model::Loader, public StagingBuffer {
public:
    VulkanGltfLoader(
        const Context& ctx,
        vk::SampleCountFlagBits sampleCount,
        MeshRegistry& meshRegistry,
        TextureRegistry& textureRegistry,
        GpuAllocator& allocator,
        PipelineFactory* pipelineFactory
    );
    ~VulkanGltfLoader() override;

    Model load(const char* path) override;

private:
    fastgltf::Parser parser;
    fastgltf::Asset asset;
    MeshRegistry& meshRegistry;
    TextureRegistry& textureRegistry;
    PipelineFactory* pipelineFactory;
    std::vector<uint8_t> data;
    vk::SampleCountFlagBits sampleCount;
    vk::Device device;

    std::tuple<dragonfire::vulkan::Mesh*, glm::vec4, vk::Fence> loadPrimitive(
        const fastgltf::Primitive& primitive,
        const fastgltf::Mesh& mesh,
        uint32_t primitiveId
    );
    std::pair<Material*, SmallVector<vk::Fence>> loadMaterial(const fastgltf::Material& material);
    void loadAsset(const char* path);
    [[nodiscard]] vk::DeviceSize computeBufferSize() const;
};

}// namespace dragonfire::vulkan