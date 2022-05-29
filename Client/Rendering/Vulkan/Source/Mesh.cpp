//
// Created by josh on 5/19/22.
//

#include "Mesh.h"

dragonfire::rendering::Mesh::Mesh(
        std::vector<Vertex>&& vertices,
        std::vector<uint32_t>&& indices,
        vk::Device device,
        vk::Queue transferQueue,
        vk::CommandBuffer cmd
)
    : BaseMesh(std::move(vertices), std::move(indices)) {
    auto vertSize = this->vertices.size() * sizeof(Vertex);
    auto indicesSize = this->indices.size() * sizeof(uint32_t);
    vk::BufferCreateInfo bufInfo{
            .size = vertSize + indicesSize,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
    };
    VmaAllocationCreateInfo allocInfo{
            .flags = VmaAllocationCreateFlagBits ::VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>(
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            ),
    };
    Buffer staging(bufInfo, allocInfo);

    char* ptr = static_cast<char*>(staging.map());
    memcpy(ptr, this->vertices.data(), vertSize);
    memcpy(ptr + vertSize, this->indices.data(), indicesSize);
    staging.unmap();
    vk::BufferCopy vertexCpy = {

            .srcOffset = 0,
            .dstOffset = 0,
            .size = vertSize,
    };
    vk::BufferCopy indexCpy = {
            .srcOffset = vertSize,
            .dstOffset = 0,
            .size = indicesSize,
    };
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    cmd.begin(beginInfo);
    cmd.copyBuffer(staging, vertexBuffer, vertexCpy);
    cmd.copyBuffer(staging, indexBuffer, indexCpy);
    cmd.end();
    vk::SubmitInfo subInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
    };
    transferQueue.submit(subInfo);
    transferQueue.waitIdle();
}
