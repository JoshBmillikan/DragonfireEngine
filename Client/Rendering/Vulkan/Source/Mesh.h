//
// Created by josh on 5/19/22.
//

#pragma once
#include <BaseMesh.h>
#include "Allocation.h"

namespace dragonfire::rendering {
class Mesh final : public BaseMesh {
    Buffer vertexBuffer, indexBuffer;
public:
    Mesh(std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, vk::Device device, vk::Queue transferQueue, vk::CommandBuffer cmd);

    void bind(vk::CommandBuffer cmd) noexcept {
        std::array<vk::DeviceSize, 1> offsets = {0};
        cmd.bindVertexBuffers(0, static_cast<vk::Buffer>(vertexBuffer), offsets);
        cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
    }
};
}