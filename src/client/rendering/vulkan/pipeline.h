//
// Created by josh on 1/19/24.
//

#pragma once
#include "vulkan_headers.h"
#include <core/utility/string_hash.h>
#include <optional>
#include <shared_mutex>
#include <spirv_reflect.h>
#include <unordered_map>
#include "descriptor_set.h"

namespace dragonfire::vulkan {

class Pipeline {
    vk::Pipeline pipeline;
    vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics;
    vk::PipelineLayout layout;

public:
    Pipeline() = default;

    Pipeline(
        const vk::Pipeline pipeline,
        const vk::PipelineBindPoint bindPoint,
        const vk::PipelineLayout layout
    )
        : pipeline(pipeline), bindPoint(bindPoint), layout(layout)
    {
    }

    void bind(const vk::CommandBuffer cmd) const { cmd.bindPipeline(bindPoint, pipeline); }

    operator vk::Pipeline() const { return pipeline; }

    [[nodiscard]] vk::PipelineBindPoint getBindPoint() const { return bindPoint; }

    [[nodiscard]] vk::PipelineLayout getLayout() const { return layout; }

    void destroy(vk::Device device);
};

struct PipelineInfo {
    enum class Type : uint8_t { GRAPHICS, COMPUTE } type = Type::GRAPHICS;
    vk::SampleCountFlagBits sampleCount;
    bool enableMultisampling = false;
    bool enableColorBlend = true;
    vk::PrimitiveTopology topology;
    vk::PipelineRasterizationStateCreateInfo rasterState;
    vk::PipelineDepthStencilStateCreateInfo depthState;
    std::string_view vertexCompShader, fragmentShader, geometryShader, tessEvalShader, tessCtrlShader;

    static size_t hash(const PipelineInfo& info) noexcept;
    bool operator==(const PipelineInfo& other) const noexcept;
};

class PipelineFactory {
public:
    PipelineFactory(const struct Context& ctx, DescriptorLayoutManager* descriptorLayoutManager);

    Pipeline getOrCreate(const PipelineInfo& info);
    std::optional<Pipeline> getPipeline(const PipelineInfo& info);

    Pipeline operator()(const PipelineInfo& info) { return getOrCreate(info); }

    PipelineFactory(const PipelineFactory& other) = delete;
    PipelineFactory(PipelineFactory&& other) noexcept = delete;
    PipelineFactory& operator=(const PipelineFactory& other) = delete;
    PipelineFactory& operator=(PipelineFactory&& other) noexcept = delete;

    void destroyShaders();

    ~PipelineFactory();

private:
    static constexpr const char* CACHE_PATH = "cache/pipeline.cache";
    static constexpr const char* SHADER_DIR = "assets/shaders";
    std::shared_mutex mutex;
    std::unordered_map<PipelineInfo, Pipeline, decltype(&PipelineInfo::hash)> pipelines;
    vk::PipelineCache cache;
    StringMap<std::pair<vk::ShaderModule, spv_reflect::ShaderModule>> shaders;
    DescriptorLayoutManager* descriptorLayoutManager = nullptr;
    vk::Device device;

    Pipeline createPipeline(const PipelineInfo& info);
    void loadShaders(const char* dir = SHADER_DIR);
    void savePipelineCache() const;
    vk::PipelineLayout createLayout(const PipelineInfo& info) const;
};

}// namespace dragonfire::vulkan
