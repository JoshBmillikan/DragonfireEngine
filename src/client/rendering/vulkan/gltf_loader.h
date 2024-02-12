//
// Created by josh on 1/29/24.
//

#pragma once
#include "allocation.h"
#include "client/rendering/model.h"
#include "core/utility/small_vector.h"
#include "mesh.h"
#include "texture.h"
#include <fastgltf/parser.hpp>

namespace dragonfire::vulkan {

class VulkanGltfLoader final : public Model::Loader {
public:
    VulkanGltfLoader(
        const Context& ctx,
        MeshRegistry& meshRegistry,
        TextureRegistry& textureRegistry,
        GpuAllocator& allocator
    );
    ~VulkanGltfLoader() override;

    Model load(const char* path) override;

private:
    fastgltf::Parser parser;
    fastgltf::Asset asset;
    Buffer stagingBuffer;
    MeshRegistry& meshRegistry;
    TextureRegistry& textureRegistry;
    GpuAllocator& allocator;
    std::vector<uint8_t> data;
    vk::Device device;

    void loadAsset(const char* path);
    [[nodiscard]] vk::DeviceSize computeBufferSize() const;
    void* getStagingPtr(vk::DeviceSize size);
};

}// namespace dragonfire::vulkan