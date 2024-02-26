//
// Created by josh on 1/29/24.
//
#define STB_IMAGE_IMPLEMENTATION
#include "gltf_loader.h"
#include "core/crash.h"
#include "core/file.h"
#include "core/utility/formatted_error.h"
#include "core/utility/small_vector.h"
#include "core/utility/temp_containers.h"
#include "core/utility/utility.h"
#include "pipeline.h"
#include "vulkan_material.h"
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
    const vk::SampleCountFlagBits sampleCount,
    MeshRegistry& meshRegistry,
    TextureRegistry& textureRegistry,
    GpuAllocator& allocator,
    PipelineFactory* pipelineFactory
)
    : StagingBuffer(allocator, 4096, false), meshRegistry(meshRegistry), textureRegistry(textureRegistry),
      pipelineFactory(pipelineFactory), sampleCount(sampleCount), device(ctx.device)
{
}

VulkanGltfLoader::~VulkanGltfLoader() = default;

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
    SmallVector<vk::Fence, 24> fences;
    Model out(std::string(asset.meshes[0].name));

    for (auto& mesh : asset.meshes) {
        uint32_t primitiveId = 0;
        for (auto& primitive : mesh.primitives) {
            auto [meshHandle, bounds, fence1] = loadPrimitive(primitive, mesh, ptr, primitiveId);
            fences.pushBack(fence1);
            // TODO material & texture data
            Material* material = nullptr;
            if (primitive.materialIndex.has_value()) {
                auto& materialInfo = asset.materials[primitive.materialIndex.value()];
                auto [mat, f] = loadMaterial(materialInfo, ptr);
                material = mat;
                for (const auto fence : f)
                    fences.pushBack(fence);
            }
            out.addPrimitive(reinterpret_cast<dragonfire::Mesh>(meshHandle), material, bounds);
            primitiveId++;
        }
    }

    if (device.waitForFences(fences.size(), fences.data(), true, UINT64_MAX) != vk::Result::eSuccess)
        SPDLOG_ERROR("Fence wait failed");
    for (const auto fence : fences)
        device.destroy(fence);
    return out;
}

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

std::tuple<dragonfire::vulkan::Mesh*, glm::vec4, vk::Fence> VulkanGltfLoader::loadPrimitive(
    const fastgltf::Primitive& primitive,
    const fastgltf::Mesh& mesh,
    void*& ptr,
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
    flushStagingBuffer();

    const size_t vertexOffset = reinterpret_cast<uintptr_t>(vertices)
                                - reinterpret_cast<uintptr_t>(getStagingBuffer().getInfo().pMappedData);
    const size_t indexOffset = reinterpret_cast<uintptr_t>(indices)
                               - reinterpret_cast<uintptr_t>(getStagingBuffer().getInfo().pMappedData);

    const auto name = primitiveId > 0 ? fmt::format("{}_{}", mesh.name, primitiveId) : std::string(mesh.name);
    const glm::vec4 bounds = computeBounds(vertices, vertexCount);
    const size_t baseOffset
        = reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(getStagingBuffer().getInfo().pMappedData);
    auto [m, fence] = meshRegistry.uploadMesh(
        name,
        getStagingBuffer(),
        vertexCount,
        indexCount,
        vertexOffset + baseOffset,
        indexOffset + baseOffset
    );

    ptr = static_cast<char*>(ptr) + vertexCount * sizeof(Vertex) + indexCount * sizeof(uint32_t);
    return {m, bounds, fence};
}

template<class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

static ImageData loadImageData(const fastgltf::DataSource& dataSource)
{
    return std::visit(
        Overloaded{
            [](auto) { return ImageData{}; },
            [](const fastgltf::sources::BufferView& buffer) {
                crash("Not yet implemented");
                return ImageData{};
            },
            [](const fastgltf::sources::URI& uri) {
                const auto s = TempString(uri.uri.path());
                File file(s.c_str());
                const auto bytes = file.read();
                file.close();
                ImageData data{};
                data.len = bytes.size();
                data.setData(
                    stbi_load_from_memory(bytes.data(), int(data.len), &data.x, &data.y, &data.channels, 4)
                );
                return data;
            },
            [](const fastgltf::sources::Vector& vector) {
                ImageData data{};
                data.len = vector.bytes.size();
                data.setData(stbi_load_from_memory(
                    vector.bytes.data(),
                    int(data.len),
                    &data.x,
                    &data.y,
                    &data.channels,
                    4
                ));
                return data;
            },
            [](const fastgltf::sources::ByteView& bytes) {
                ImageData data{};
                data.len = bytes.bytes.size_bytes();
                data.setData(stbi_load_from_memory(
                    reinterpret_cast<const uint8_t*>(bytes.bytes.data()),
                    int(data.len),
                    &data.x,
                    &data.y,
                    &data.channels,
                    4
                ));
                return data;
            }
        },
        dataSource
    );
}

std::pair<Material*, SmallVector<vk::Fence>> VulkanGltfLoader::loadMaterial(
    const fastgltf::Material& material,
    void*& ptr
)
{
    PipelineInfo pipelineInfo;
    pipelineInfo.sampleCount = sampleCount;
    pipelineInfo.enableMultisampling = sampleCount != vk::SampleCountFlagBits::e1;
    pipelineInfo.topology = vk::PrimitiveTopology::eTriangleList;

    pipelineInfo.rasterState.frontFace = vk::FrontFace::eCounterClockwise;
    pipelineInfo.rasterState.cullMode = vk::CullModeFlagBits::eBack;
    pipelineInfo.rasterState.polygonMode = vk::PolygonMode::eFill;
    pipelineInfo.rasterState.lineWidth = 1.0f;

    pipelineInfo.depthState.depthWriteEnable = pipelineInfo.depthState.depthTestEnable = true;
    pipelineInfo.depthState.depthCompareOp = vk::CompareOp::eLessOrEqual;
    pipelineInfo.depthState.minDepthBounds = 0.0f;
    pipelineInfo.depthState.maxDepthBounds = 1.0f;

    // TODO shaders

    const Pipeline pipeline = pipelineFactory->getOrCreate(pipelineInfo);
    if (material.pbrData.baseColorTexture.has_value()) {
        auto& texture = asset.textures[material.pbrData.baseColorTexture.value().textureIndex];
        auto& image = asset.images[texture.imageIndex.value()];
        auto name = texture.name.empty() ? image.name : texture.name;
        ImageData imageData = loadImageData(image.data);
        // todo
    }
    const TextureIds textureIds{

    };

    auto out = new VulkanMaterial(textureIds, pipeline);

    return {out, {}};
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

}// namespace dragonfire::vulkan