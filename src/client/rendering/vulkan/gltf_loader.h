//
// Created by josh on 1/29/24.
//

#pragma once
#include "allocation.h"
#include "client/rendering/base_gltf_loader.h"
#include "mesh.h"
#include "texture.h"

namespace dragonfire::vulkan {

class VulkanGltfLoader final : public GltfLoader {
public:
    VulkanGltfLoader(
        const Context& ctx,
        MeshRegistry& meshRegistry,
        TextureRegistry& textureRegistry,
        GpuAllocator& allocator
    );
    ~VulkanGltfLoader() override;

    std::pair<dragonfire::Mesh, Material> load(const char* path) override;

private:
    Buffer stagingBuffer;
    MeshRegistry& meshRegistry;
    TextureRegistry& textureRegistry;
    GpuAllocator& allocator;
    std::vector<uint8_t> data;
    vk::CommandPool pool;
    vk::CommandBuffer cmd;
    vk::Device device;

    void loadAsset(const char* path);
    [[nodiscard]] vk::DeviceSize computeBufferSize() const;
    void* getStagingPtr(vk::DeviceSize size);
};

}// namespace dragonfire::vulkan