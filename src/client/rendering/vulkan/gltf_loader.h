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

#include <fastgltf/core.hpp>
#include <functional>

namespace dragonfire::vulkan {

class VulkanGltfLoader final : public Model::Loader, public StagingBuffer {
public:
    VulkanGltfLoader(
        const Context& ctx,
        MeshRegistry& meshRegistry,
        TextureRegistry& textureRegistry,
        GpuAllocator& allocator,
        PipelineFactory* pipelineFactory,
        std::function<void(Texture*)>&& descriptorUpdateCallback
    );
    ~VulkanGltfLoader() override = default;
    std::span<const char*> acceptedFileExtensions() override;
    Model* load(const char* path) override;
    VulkanGltfLoader(const VulkanGltfLoader& other) = delete;
    VulkanGltfLoader(VulkanGltfLoader&& other) noexcept = delete;
    VulkanGltfLoader& operator=(const VulkanGltfLoader& other) = delete;
    VulkanGltfLoader& operator=(VulkanGltfLoader&& other) noexcept = delete;

private:
    fastgltf::Parser parser;
    fastgltf::Asset asset;
    MeshRegistry& meshRegistry;
    TextureRegistry& textureRegistry;
    PipelineFactory* pipelineFactory;
    std::vector<uint8_t> data;
    vk::SampleCountFlagBits sampleCount;
    vk::Device device;
    std::function<void(Texture*)> descriptorUpdateCallback;

    std::tuple<dragonfire::vulkan::Mesh*, glm::vec4, vk::Fence> loadPrimitive(
        const fastgltf::Primitive& primitive,
        const fastgltf::Mesh& mesh,
        uint32_t primitiveId
    );
    std::pair<Material*, SmallVector<vk::Fence>> loadMaterial(const fastgltf::Material& material);
    void loadAsset(const char* path);
    void loadMesh(
        const fastgltf::Mesh& mesh,
        Model& out,
        const glm::mat4& transform = glm::identity<glm::mat4>()
    );
    void loadNode(
        const fastgltf::Node& node,
        Model& out,
        const glm::mat4& transform = glm::identity<glm::mat4>()
    );
    Texture* loadTexture(const fastgltf::TextureInfo& textureInfo);
};

}// namespace dragonfire::vulkan