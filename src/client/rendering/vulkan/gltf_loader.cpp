//
// Created by josh on 1/29/24.
//
#include "gltf_loader.h"
#include "core/file.h"
#include "core/utility/formatted_error.h"
#include "core/utility/small_vector.h"
#include "core/utility/utility.h"
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/glm.hpp>
#include <meshoptimizer.h>
#include <physfs.h>
#include <spdlog/spdlog.h>

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

VulkanGltfLoader::~VulkanGltfLoader() = default;

static void optimizeMesh(
    Vertex* vertices,
    size_t& vertexCount,
    uint32_t* indices,
    const size_t indexCount,
    void* ptr
)
{
    const size_t baseVertexCount = vertexCount;
    std::vector<unsigned int> remap(std::max(baseVertexCount, indexCount));
    vertexCount = meshopt_generateVertexRemap(
        &remap[0],
        indices,
        indexCount,
        vertices,
        baseVertexCount,
        sizeof(Vertex)
    );
    meshopt_remapVertexBuffer(vertices, vertices, baseVertexCount, sizeof(Vertex), &remap[0]);
    const uint32_t* oldIndices = indices;
    indices = reinterpret_cast<uint32_t*>(static_cast<char*>(ptr) + sizeof(Vertex) * vertexCount);
    meshopt_remapIndexBuffer(indices, oldIndices, indexCount, &remap[0]);
    meshopt_optimizeVertexCache(indices, indices, indexCount, vertexCount);
    meshopt_optimizeOverdraw(
        indices,
        indices,
        indexCount,
        &vertices[0].position.x,
        vertexCount,
        sizeof(Vertex),
        1.05f
    );
    meshopt_optimizeVertexFetch(vertices, indices, indexCount, vertices, vertexCount, sizeof(Vertex));
}

glm::vec4 computeBounds(const Vertex* vertices, const uint32_t vertexCount)
{
    glm::vec4 out{};
    for (uint32_t i = 0; i < vertexCount; i++) {
        out.x += vertices[i].position.x;
        out.y += vertices[i].position.y;
        out.z += vertices[i].position.z;
    }
    out /= vertexCount;
    for (uint32_t i = 0; i < vertexCount; i++) {
        glm::vec3 center = out;
        const float distance = std::abs(glm::distance(center, vertices[i].position));
        if (distance > out.w)
            out.w = distance;
    }
    return out;
}

Model VulkanGltfLoader::load(const char* path)
{
    loadAsset(path);
    const vk::DeviceSize totalSize = computeBufferSize();
    void* ptr = getStagingPtr(totalSize);
    SmallVector<vk::Fence, 12> fences;
    Model out(std::string(asset.meshes[0].name));

    for (auto& mesh : asset.meshes) {
        uint32_t primitiveId = 0;
        for (auto& primitive : mesh.primitives) {
            fences.pushBack(loadPrimitive(primitive, mesh, out, ptr, primitiveId));
            primitiveId++;
        }

        // TODO material & texture data
    }

    if (device.waitForFences(fences.size(), fences.data(), true, UINT64_MAX) != vk::Result::eSuccess)
        SPDLOG_ERROR("Fence wait failed");
    for (const auto fence : fences)
        device.destroy(fence);
    return out;
}

vk::Fence VulkanGltfLoader::loadPrimitive(
    const fastgltf::Primitive& primitive,
    const fastgltf::Mesh& mesh,
    Model model,
    void* ptr,
    uint32_t primitiveId
) const
{
    const auto vertices = static_cast<Vertex*>(ptr);
    size_t vertexCount = 0;
    const auto& posAccessor = asset.accessors[primitive.findAttribute("POSITION")->second];
    fastgltf::iterateAccessor<glm::vec3>(asset, posAccessor, [&](const glm::vec3 pos) {
        vertices[vertexCount++] = Vertex{.position = pos, .normal = {1, 0, 0}, .uv = {0, 0}};
    });

    const auto normals = primitive.findAttribute("NORMAL");
    if (normals != primitive.attributes.end()) {
        const auto normAccessor = asset.accessors[normals->second];
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset,
            normAccessor,
            [=](const glm::vec3 norm, const size_t index) { vertices[index].normal = norm; }
        );
    }
    else
        SPDLOG_WARN("Mesh {} is missing vertex normals", mesh.name);

    const auto texCords = primitive.findAttribute("TEXCOORD_0");
    if (texCords != primitive.attributes.end()) {
        const auto& uvAccessor = asset.accessors[texCords->second];
        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset,
            uvAccessor,
            [=](const glm::vec2 uv, const size_t index) { vertices[index].uv = uv; }
        );
    }

    auto indices = reinterpret_cast<uint32_t*>(static_cast<char*>(ptr) + sizeof(Vertex) * vertexCount);
    size_t indexCount = 0;
    if (primitive.indicesAccessor.has_value()) {
        const auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
        fastgltf::copyFromAccessor<uint32_t>(asset, indicesAccessor, indices + indexCount);
        indexCount += indicesAccessor.count;
    }
    if (optimizeMeshes)
        optimizeMesh(vertices, vertexCount, indices, indexCount, ptr);
    stagingBuffer.flush();

    const size_t vertexOffset = reinterpret_cast<uintptr_t>(vertices)
                                - reinterpret_cast<uintptr_t>(stagingBuffer.getInfo().pMappedData);
    const size_t indexOffset = reinterpret_cast<uintptr_t>(indices)
                               - reinterpret_cast<uintptr_t>(stagingBuffer.getInfo().pMappedData);

    const auto name = primitiveId > 0 ? fmt::format("{}_{}", mesh.name, primitiveId) : std::string(mesh.name);
    const glm::vec4 bounds = computeBounds(vertices, vertexCount);
    auto [m, fence]
        = meshRegistry.uploadMesh(name, stagingBuffer, vertexCount, indexCount, vertexOffset, indexOffset);
    model.addPrimitive(dragonfire::Mesh(m), nullptr, bounds);

    ptr = static_cast<char*>(ptr) + vertexCount * sizeof(Vertex) + indexCount * sizeof(uint32_t);
    return fence;
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