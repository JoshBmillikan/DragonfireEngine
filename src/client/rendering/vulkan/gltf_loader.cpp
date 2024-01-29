//
// Created by josh on 1/29/24.
//

#include "gltf_loader.h"
#include "core/file.h"
#include "core/utility/formatted_error.h"
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
}

VulkanGltfLoader::~VulkanGltfLoader()
{
    device.destroy(pool);
    device.destroy(fence);
}

std::pair<dragonfire::Mesh, Material> VulkanGltfLoader::load(const char* path)
{
    loadAsset(path);

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
}// namespace dragonfire::vulkan