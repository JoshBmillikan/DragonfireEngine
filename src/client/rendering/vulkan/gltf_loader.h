//
// Created by josh on 1/29/24.
//

#pragma once
#include "allocation.h"
#include "client/rendering/model.h"
#include "core/utility/small_vector.h"
#include "mesh.h"
#include "pipeline.h"
#include "texture.h"
#include <fastgltf/parser.hpp>

namespace dragonfire::vulkan {

class VulkanGltfLoader final : public Model::Loader {
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
    Buffer stagingBuffer;
    MeshRegistry& meshRegistry;
    TextureRegistry& textureRegistry;
    PipelineFactory* pipelineFactory;
    GpuAllocator& allocator;
    std::vector<uint8_t> data;
    vk::SampleCountFlagBits sampleCount;
    vk::Device device;

    std::tuple<dragonfire::vulkan::Mesh*, glm::vec4, vk::Fence> loadPrimitive(
        const fastgltf::Primitive& primitive,
        const fastgltf::Mesh& mesh,
        void*& ptr,
        uint32_t primitiveId
    ) const;
    std::pair<Material*, SmallVector<vk::Fence>> loadMaterial(const fastgltf::Material& material, void*& ptr);
    void loadAsset(const char* path);
    [[nodiscard]] vk::DeviceSize computeBufferSize() const;
    void* getStagingPtr(vk::DeviceSize size);
};

}// namespace dragonfire::vulkan