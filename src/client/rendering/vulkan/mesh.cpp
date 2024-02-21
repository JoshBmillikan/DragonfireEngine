//
// Created by josh on 1/20/24.
//

#include "mesh.h"
#include "core/config.h"
#include <spdlog/spdlog.h>
#include <utility>

namespace dragonfire::vulkan {

MeshRegistry::MeshRegistry(const Context& ctx, GpuAllocator& allocator)
    : allocator(allocator), device(ctx.device), queue(ctx.queues.transfer)
{
    const auto& cfg = Config::get();
    maxVertexCount = cfg.getInt("maxVertexCount").value_or(1 << 26);
    maxIndexCount = cfg.getInt("maxIndexCount").value_or(1 << 27);
    const auto logger = spdlog::get("Rendering");
    SPDLOG_LOGGER_TRACE(
        logger,
        "Using max vertex count of {} and max index count of {}",
        maxVertexCount,
        maxIndexCount
    );

    vk::BufferCreateInfo createInfo{};
    createInfo.size = sizeof(Vertex) * maxVertexCount;
    createInfo.sharingMode = vk::SharingMode::eExclusive;
    createInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer
                       | vk::BufferUsageFlagBits::eStorageBuffer;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.priority = 1.0f;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vertexBuffer = this->allocator.allocate(createInfo, allocInfo);

    createInfo.size = sizeof(uint32_t) * maxIndexCount;
    createInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer
                       | vk::BufferUsageFlagBits::eStorageBuffer;
    indexBuffer = this->allocator.allocate(createInfo, allocInfo);
    VmaVirtualBlockCreateInfo blockCreateInfo{};
    blockCreateInfo.size = vertexBuffer.getInfo().size;
    VkResult result = vmaCreateVirtualBlock(&blockCreateInfo, &vertexBlock);
    if (result == VK_SUCCESS) {
        blockCreateInfo.size = indexBuffer.getInfo().size;
        result = vmaCreateVirtualBlock(&blockCreateInfo, &indexBlock);
    }
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create virtual blocks for vertex/index buffers");

    vk::CommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.queueFamilyIndex = ctx.queues.transferFamily;
    pool = device.createCommandPool(poolCreateInfo);

    vk::CommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    cmdAllocInfo.commandBufferCount = 1;
    cmdAllocInfo.commandPool = pool;
    vk::resultCheck(device.allocateCommandBuffers(&cmdAllocInfo, &cmd), "Failed to allocate command buffer");
}

MeshRegistry::~MeshRegistry()
{
    vmaClearVirtualBlock(vertexBlock);
    vmaClearVirtualBlock(indexBlock);
    vmaDestroyVirtualBlock(vertexBlock);
    vmaDestroyVirtualBlock(indexBlock);
    device.destroy(pool);
}

std::pair<Mesh*, vk::Fence> MeshRegistry::uploadMesh(
    const std::string& id,
    const Buffer& stagingBuffer,
    const size_t vertexCount,
    const size_t indexCount,
    size_t indexOffset,
    const size_t vertexOffset
)
{
    if (indexOffset == 0)
        indexOffset = vertexCount * sizeof(Vertex);

    {
        std::shared_lock lock(mutex);
        if (meshes.contains(id))
            return {&meshes[id], nullptr};
    }
    const vk::DeviceSize vertexSize = vertexCount * sizeof(Vertex);
    const vk::DeviceSize indexSize = indexCount * sizeof(uint32_t);

    VmaVirtualAllocationCreateInfo vertexAllocInfo{}, indexAllocInfo{};
    vertexAllocInfo.size = vertexSize;
    vertexAllocInfo.alignment = 16;
    indexAllocInfo.size = indexSize;
    indexAllocInfo.alignment = 16;

    std::unique_lock lock(mutex);
    Mesh* mesh = &meshes[id];
    mesh->vertexCount = vertexCount;
    mesh->indexCount = indexCount;
    VkResult result = vmaVirtualAllocate(vertexBlock, &vertexAllocInfo, &mesh->vertexAlloc, nullptr);
    if (result == VK_SUCCESS)
        result = vmaVirtualAllocate(indexBlock, &indexAllocInfo, &mesh->indexAlloc, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("VMA virtual allocation failed");
    }
    vmaGetVirtualAllocationInfo(vertexBlock, mesh->vertexAlloc, &mesh->vertexInfo);
    vmaGetVirtualAllocationInfo(vertexBlock, mesh->vertexAlloc, &mesh->indexInfo);

    device.resetCommandPool(pool);
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmd.begin(beginInfo);
    vk::BufferCopy vertexCopy;
    vertexCopy.size = vertexSize;
    vertexCopy.srcOffset = vertexOffset;
    vertexCopy.dstOffset = mesh->vertexInfo.offset;

    cmd.copyBuffer(stagingBuffer, vertexBuffer, vertexCopy);

    vk::BufferCopy indexCopy;
    indexCopy.size = indexSize;
    indexCopy.srcOffset = indexOffset;
    indexCopy.dstOffset = mesh->indexInfo.offset;

    cmd.copyBuffer(stagingBuffer, indexBuffer, indexCopy);

    vk::SubmitInfo submitInfo{};
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.commandBufferCount = 1;
    cmd.end();

    vk::Fence fence = device.createFence(vk::FenceCreateInfo());

    try {
        queue.submit(submitInfo, fence);
    }
    catch (...) {
        device.destroy(fence);
        throw;
    }

    return {mesh, fence};
}

std::pair<Mesh*, vk::Fence> MeshRegistry::uploadMesh(
    const std::string& id,
    const std::span<Vertex> vertices,
    const std::span<uint32_t> indices
)
{
    {
        std::shared_lock lock(mutex);
        if (meshes.contains(id))
            return {&meshes[id], nullptr};
    }
    vk::BufferCreateInfo createInfo{};
    createInfo.size = vertices.size_bytes() + indices.size_bytes();
    createInfo.sharingMode = vk::SharingMode::eExclusive;
    createInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.priority = 1.0f;
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    const Buffer stagingBuffer = allocator.allocate(createInfo, allocInfo);
    auto ptr = static_cast<char*>(stagingBuffer.getInfo().pMappedData);
    memcpy(ptr, vertices.data(), vertices.size_bytes());
    ptr += vertices.size_bytes();
    memcpy(ptr, indices.data(), indices.size_bytes());

    return uploadMesh(id, stagingBuffer, vertices.size(), indices.size());
}

void MeshRegistry::freeMesh(const Mesh* mesh)
{
    if (mesh) {
        std::unique_lock lock(mutex);
        vmaVirtualFree(vertexBlock, mesh->vertexAlloc);
        vmaVirtualFree(indexBlock, mesh->indexAlloc);
    }
}

Mesh* MeshRegistry::getMesh(const std::string_view id)
{
    std::shared_lock lock(mutex);
    const auto iter = meshes.find(id);
    if (iter == meshes.end())
        return nullptr;
    return &iter->second;
}

void MeshRegistry::bindBuffers(const vk::CommandBuffer cmd)
{
    cmd.bindVertexBuffers(0, {vertexBuffer}, {0});
    cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
}

}// namespace dragonfire::vulkan
