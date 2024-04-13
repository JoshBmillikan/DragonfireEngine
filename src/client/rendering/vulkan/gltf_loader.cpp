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
#include "pipeline.h"
#include "vulkan_material.h"
// ReSharper disable once CppUnusedIncludeDirective
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <meshoptimizer.h>
#include <physfs.h>
#include <re2/re2.h>
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
    GpuAllocator& allocator,
    PipelineFactory* pipelineFactory,
    std::function<void(Texture*)>&& descriptorUpdateCallback
)
    : StagingBuffer(allocator, 4096, false, "mesh staging buffer"), meshRegistry(meshRegistry),
      textureRegistry(textureRegistry), pipelineFactory(pipelineFactory), sampleCount(ctx.sampleCount),
      device(ctx.device), descriptorUpdateCallback(descriptorUpdateCallback)
{
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
    Model out(std::string(asset.meshes[0].name));
    if (asset.defaultScene.has_value()) {
        const auto& scene = asset.scenes[asset.defaultScene.value()];
        if (!scene.nodeIndices.empty()) {
            for (const auto node : scene.nodeIndices)
                loadNode(asset.nodes[node], out);
            return out;
        }
    }
    for (auto& mesh : asset.meshes)
        loadMesh(mesh, out);
    return out;
}

void VulkanGltfLoader::loadMesh(const fastgltf::Mesh& mesh, Model& out, const glm::mat4& transform)
{
    uint32_t primitiveId = 0;
    for (auto& primitive : mesh.primitives) {
        auto [meshHandle, bounds, fence] = loadPrimitive(primitive, mesh, primitiveId);
        if (device.waitForFences(fence, true, UINT64_MAX) != vk::Result::eSuccess)
            SPDLOG_ERROR("Fence wait failed");
        device.destroy(fence);

        auto material = Material::DEFAULT;
        if (primitive.materialIndex.has_value()) {
            auto& materialInfo = asset.materials[primitive.materialIndex.value()];
            auto [mat, f] = loadMaterial(materialInfo);
            material = std::shared_ptr<Material>(mat);
            if (!f.empty()) {
                if (device.waitForFences(f.size(), f.data(), true, UINT64_MAX) != vk::Result::eSuccess)
                    SPDLOG_ERROR("Fence wait failed");
                for (const auto fn : f)
                    device.destroy(fn);
            }
        }
        out.addPrimitive(Model::Primitive{
            reinterpret_cast<dragonfire::Mesh>(meshHandle),
            material,
            bounds,
            transform,
        });
        primitiveId++;
    }
}

template<class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

void VulkanGltfLoader::loadNode(const fastgltf::Node& node, Model& out, const glm::mat4& transform)
{
    const glm::mat4 mat = std::visit(
        Overloaded{
            [&](const fastgltf::Node::TRS& trs) {
                return transform
                       * glm::translate(glm::identity<glm::mat4>(), glm::make_vec3(trs.translation.data()))
                       * glm::toMat4(
                           glm::quat(trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3])
                       )
                       * glm::scale(glm::identity<glm::mat4>(), glm::make_vec3(trs.scale.data()));
            },
            [&](const fastgltf::Node::TransformMatrix& matrix) {
                return transform * glm::make_mat4(matrix.data());
            }
        },
        node.transform
    );
    if (node.meshIndex.has_value()) {
        const auto& mesh = asset.meshes[node.meshIndex.value()];
        loadMesh(mesh, out, mat);
    }
    for (const auto child : node.children)
        loadNode(asset.nodes[child], out, mat);
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
    uint32_t primitiveId
)
{
    const auto& posAccessor = asset.accessors[primitive.findAttribute("POSITION")->second];
    vk::DeviceSize size = posAccessor.count * sizeof(Vertex);
    if (primitive.indicesAccessor.has_value()) {
        const auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
        size += indicesAccessor.count * sizeof(uint32_t);
    }
    void* ptr = getStagingPtr(size);
    const auto vertices = static_cast<Vertex*>(ptr);
    size_t vertexCount = 0;
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

    const auto indices
        = reinterpret_cast<uint32_t*>(static_cast<unsigned char*>(ptr) + sizeof(Vertex) * vertexCount);
    size_t indexCount = 0;
    if (primitive.indicesAccessor.has_value()) {
        const auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
        fastgltf::copyFromAccessor<uint32_t>(asset, indicesAccessor, indices);
        indexCount += indicesAccessor.count;
    }
    if (optimizeMeshes)
        optimizeMesh(vertices, vertexCount, indexCount > 0 ? indices : nullptr, indexCount, ptr);
    flushStagingBuffer();

    const size_t vertexOffset = vertexCount * sizeof(Vertex);

    const auto name = primitiveId > 0 ? fmt::format("{}_{}", mesh.name, primitiveId) : std::string(mesh.name);
    const glm::vec4 bounds = computeBounds(vertices, vertexCount);

    auto [m, fence]
        = meshRegistry.uploadMesh(name, getStagingBuffer(), vertexCount, indexCount, vertexOffset, 0);

    return {m, bounds, fence};
}

static ImageData loadImageData(
    const fastgltf::DataSource& dataSource,
    const fastgltf::Asset& asset,
    size_t byteOffset = 0,
    size_t byteLen = 0
)
{
    return std::visit(
        Overloaded{
            [&](const fastgltf::sources::BufferView& buffer) {
                auto& view = asset.bufferViews[buffer.bufferViewIndex];
                auto& buf = asset.buffers[view.bufferIndex];
                assert(!std::holds_alternative<fastgltf::sources::BufferView>(buf.data));
                return loadImageData(buf.data, asset, view.byteOffset, view.byteLength);
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
                if (data.data == nullptr)
                    throw FormattedError("Failed to load texture image, reason: {}", stbi_failure_reason());
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
                if (data.data == nullptr)
                    throw FormattedError("Failed to load texture image, reason: {}", stbi_failure_reason());
                return data;
            },
            [byteOffset, byteLen](const fastgltf::sources::ByteView& bytes) {
                ImageData data{};
                data.len = byteLen > 0 ? byteLen : bytes.bytes.size_bytes();
                data.setData(stbi_load_from_memory(
                    reinterpret_cast<const uint8_t*>(bytes.bytes.data() + byteOffset),
                    int(data.len),
                    &data.x,
                    &data.y,
                    &data.channels,
                    4
                ));
                if (data.data == nullptr)
                    throw FormattedError("Failed to load texture image, reason: {}", stbi_failure_reason());
                return data;
            },
            [](auto) {
                crash("Invalid data source for texture image");
                return ImageData{};
            }
        },
        dataSource
    );
}

static RE2 VERTEX_REGEX("vs-([a-zA-Z_0-9]+)", RE2::Quiet);
static RE2 FRAG_REGEX("fs-([a-zA-Z_0-9]+)", RE2::Quiet);
static RE2 GEOM_REGEX("geom-([a-zA-Z_0-9]+)", RE2::Quiet);
static RE2 TESS_REGEX("teseval-([a-zA-Z_0-9]+).*tesctrl=(a-zA-Z_0-9]+)", RE2::Quiet);

std::pair<Material*, SmallVector<vk::Fence>> VulkanGltfLoader::loadMaterial(const fastgltf::Material& material
)
{
    PipelineInfo pipelineInfo{};
    pipelineInfo.enableColorBlend = false;
    pipelineInfo.sampleCount = sampleCount;
    pipelineInfo.enableMultisampling = sampleCount != vk::SampleCountFlagBits::e1;
    pipelineInfo.topology = vk::PrimitiveTopology::eTriangleList;

    pipelineInfo.rasterState.frontFace = vk::FrontFace::eCounterClockwise;
    pipelineInfo.rasterState.cullMode = vk::CullModeFlagBits::eBack;
    pipelineInfo.rasterState.polygonMode = vk::PolygonMode::eFill;
    pipelineInfo.rasterState.lineWidth = 1.0f;

    pipelineInfo.depthState.depthWriteEnable = pipelineInfo.depthState.depthTestEnable = true;
    pipelineInfo.depthState.depthCompareOp = vk::CompareOp::eLessOrEqual;
    pipelineInfo.depthState.depthBoundsTestEnable = false;
    pipelineInfo.depthState.stencilTestEnable = false;
    pipelineInfo.depthState.minDepthBounds = 0.0f;
    pipelineInfo.depthState.maxDepthBounds = 1.0f;

    RE2::FullMatch(material.name, VERTEX_REGEX, &pipelineInfo.vertexCompShader);
    RE2::FullMatch(material.name, FRAG_REGEX, &pipelineInfo.fragmentShader);
    RE2::FullMatch(material.name, GEOM_REGEX, &pipelineInfo.geometryShader);
    RE2::FullMatch(material.name, TESS_REGEX, &pipelineInfo.tessEvalShader, &pipelineInfo.tessCtrlShader);

    if (!pipelineInfo.vertexCompShader.empty())
        pipelineInfo.vertexCompShader += ".vert";
    else
        pipelineInfo.vertexCompShader = "default.vert";
    if (!pipelineInfo.fragmentShader.empty())
        pipelineInfo.fragmentShader += ".frag";
    else
        pipelineInfo.fragmentShader = "default.frag";
    if (!pipelineInfo.geometryShader.empty())
        pipelineInfo.geometryShader += ".geom";
    if (!pipelineInfo.tessEvalShader.empty())
        pipelineInfo.tessEvalShader += ".tese";
    if (!pipelineInfo.tessCtrlShader.empty())
        pipelineInfo.tessCtrlShader += ".tesc";

    const Pipeline pipeline = pipelineFactory->getOrCreate(pipelineInfo);


    TextureIds textureIds{};
    if (material.pbrData.baseColorTexture.has_value())
        textureIds.albedo = loadTexture(material.pbrData.baseColorTexture.value())->getId();
    if (material.pbrData.metallicRoughnessTexture.has_value())
        textureIds.metallic = loadTexture(material.pbrData.metallicRoughnessTexture.value())->getId();
    if (material.normalTexture.has_value())
        textureIds.normal = loadTexture(material.normalTexture.value())->getId();
    if (material.emissiveTexture.has_value())
        textureIds.emmisive = loadTexture(material.emissiveTexture.value())->getId();

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
    constexpr auto opts = fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers
                          | fastgltf::Options::DecomposeNodeMatrices;
    const auto type = determineGltfFileType(&buffer);
    asset = std::move(checkGltf(
        type == fastgltf::GltfType::glTF ? parser.loadGLTF(&buffer, PHYSFS_getRealDir(path), opts)
                                         : parser.loadBinaryGLTF(&buffer, PHYSFS_getRealDir(path), opts)
    ));
    SPDLOG_TRACE("Loaded asset file \"{}\"", path);
}

Texture* VulkanGltfLoader::loadTexture(const fastgltf::TextureInfo& textureInfo)
{
    auto& texture = asset.textures[textureInfo.textureIndex];
    const auto& image = asset.images[texture.imageIndex.value()];
    const auto name = texture.name.empty() ? image.name : texture.name;
    ImageData imageData = loadImageData(image.data, asset);
    Texture* t = textureRegistry.getCreateTexture(name, std::move(imageData));
    descriptorUpdateCallback(t);
    return t;
}

}// namespace dragonfire::vulkan