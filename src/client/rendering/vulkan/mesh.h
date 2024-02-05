//
// Created by josh on 1/20/24.
//

#pragma once
#include "allocation.h"
#include "context.h"
#include <client/rendering/vertex.h>
#include <core/utility/string_hash.h>
#include <shared_mutex>

namespace dragonfire::vulkan {

struct Mesh {
    vk::DeviceSize vertexCount, indexCount;
    VmaVirtualAllocation vertexAlloc, indexAlloc;
    VmaVirtualAllocationInfo vertexInfo, indexInfo;
};

class MeshRegistry {
public:
    MeshRegistry(const Context& ctx, GpuAllocator& allocator);
    ~MeshRegistry();
    std::pair<Mesh*, vk::Fence> uploadMesh(
        const std::string& id,
        const Buffer& stagingBuffer,
        size_t vertexCount,
        size_t indexCount,
        size_t indexOffset = vertexCount * sizeof(Vertex)
    );
    std::pair<Mesh*, vk::Fence> uploadMesh(
        const std::string& id,
        std::span<Vertex> vertices,
        std::span<uint32_t> indices
    );
    void freeMesh(const Mesh* mesh);
    Mesh* getMesh(std::string_view id);

private:
    std::shared_mutex mutex;
    StringMap<Mesh> meshes;
    size_t maxVertexCount, maxIndexCount;
    Buffer vertexBuffer, indexBuffer;
    VmaVirtualBlock vertexBlock{}, indexBlock{};
    GpuAllocator& allocator;
    vk::Device device;
    vk::CommandPool pool;
    vk::CommandBuffer cmd;
    vk::Queue queue;
};

}// namespace dragonfire::vulkan
