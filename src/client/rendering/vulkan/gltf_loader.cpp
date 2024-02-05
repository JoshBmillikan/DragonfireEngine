//
// Created by josh on 1/29/24.
//

#include "gltf_loader.h"
#include "core/file.h"
#include "core/utility/formatted_error.h"
#include "core/utility/utility.h"
#include <fastgltf/tools.hpp>

#include <physfs.h>

namespace dragonfire::vulkan {

template<typename T>
static T& checkGltf(fastgltf::Expected<T>&& expected)
{
    if (expected.error() != fastgltf::Error::None)
        throw FormattedError("Fastgltf error: {}", fastgltf::getErrorMessage(expected.error()));
    return expected.get();
}

VulkanGltfLoader::VulkanGltfLoader(
    const Context& ctx,
    MeshRegistry& meshRegistry,
    TextureRegistry& textureRegistry,
    GpuAllocator& allocator
)
    : meshRegistry(meshRegistry), textureRegistry(textureRegistry), allocator(allocator), device(ctx.device)
{
    fence = ctx.device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
    vk::CommandPoolCreateInfo createInfo{};
    createInfo.queueFamilyIndex = ctx.queues.transferFamily;
    pool = device.createCommandPool(createInfo);
    vk::CommandBufferAllocateInfo cmdInfo{};
    cmdInfo.level = vk::CommandBufferLevel::ePrimary;
    cmdInfo.commandPool = pool;
    cmdInfo.commandBufferCount = 1;
    resultCheck(device.allocateCommandBuffers(&cmdInfo, &cmd), "Failed to allocate command buffer");

    vk::BufferCreateInfo bufInfo{};
    bufInfo.size = 4096;
    bufInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    bufInfo.sharingMode = vk::SharingMode::eExclusive;
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocInfo.priority = 1.0f;
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    stagingBuffer = allocator.allocate(bufInfo, allocInfo);
}

VulkanGltfLoader::~VulkanGltfLoader()
{
    device.destroy(pool);
    device.destroy(fence);
}

std::pair<dragonfire::Mesh, Material> VulkanGltfLoader::load(const char* path)
{
    loadAsset(path);
    const vk::DeviceSize totalSize = computeBufferSize();
    void* ptr = getStagingPtr(totalSize);
    for (auto& mesh : asset.meshes) {
        for (auto& primitive : mesh.primitives) {
            if (primitive.indicesAccessor.has_value()) {
                auto& indices = asset.accessors[primitive.indicesAccessor.value()];
                fastgltf::copyFromAccessor<uint32_t>(asset, indices, ptr);
            }
        }
    }


    // TODO
}

void VulkanGltfLoader::loadAsset(const char* path)
{
    data.clear();
    File file(path);
    data = file.read();
    file.close();
    const auto size = data.size();
    data.resize(data.size() + fastgltf::getGltfBufferPadding());
    fastgltf::GltfDataBuffer buffer;
    buffer.fromByteView(data.data(), size, data.capacity());
    constexpr auto opts = fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers;
    const auto type = determineGltfFileType(&buffer);
    asset = std::move(checkGltf(
        type == fastgltf::GltfType::glTF ? parser.loadGLTF(&buffer, PHYSFS_getRealDir(path), opts)
                                         : parser.loadBinaryGLTF(&buffer, PHYSFS_getRealDir(path), opts)
    ));
}

vk::DeviceSize VulkanGltfLoader::computeBufferSize() const
{
    vk::DeviceSize size = 0;
    for (const auto& b : asset.buffers)
        size += b.byteLength;
    return padToAlignment(size, 16);
}

void* VulkanGltfLoader::getStagingPtr(const vk::DeviceSize size)
{
    if (stagingBuffer.getInfo().size < size) {
        vk::BufferCreateInfo createInfo{};
        createInfo.size = size;
        createInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.sharingMode = vk::SharingMode::eExclusive;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.priority = 1.0f;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        stagingBuffer = allocator.allocate(createInfo, allocInfo);
    }
    return stagingBuffer.getInfo().pMappedData;
}
}// namespace dragonfire::vulkan