//
// Created by josh on 1/19/24.
//

#include "pipeline.h"
#include "client/rendering/vertex.h"
#include "context.h"
#include "core/crash.h"
#include "core/file.h"
#include "core/utility/utility.h"
#include <core/utility/small_vector.h>
#include <physfs.h>
#include <ranges>
#include <set>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_hash.hpp>
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace dragonfire::vulkan {

constexpr int MAX_SHADER_COUNT = 5;

static std::array MESH_VERTEX_INPUT_BINDING = {
    vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex),
};

static std::array MESH_VERTEX_ATTRIBUTES = {
    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)),
    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
    vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)),
};

void Pipeline::destroy(const vk::Device device)
{
    device.destroy(layout);
    device.destroy(pipeline);
    pipeline = nullptr;
    layout = nullptr;
}

size_t PipelineInfo::hash(const PipelineInfo& info) noexcept
{
    size_t hash = std::hash<uint8_t>()(static_cast<uint8_t>(info.type));
    hashCombine(hash, info.sampleCount);
    hashCombine(hash, info.enableMultisampling);
    hashCombine(hash, info.enableColorBlend);
    hashCombine(hash, info.topology);
    hashCombine(hash, info.rasterState);
    hashCombine(hash, info.depthState);
    hashCombine(hash, info.vertexCompShader);
    hashCombine(hash, info.fragmentShader);
    hashCombine(hash, info.geometryShader);
    hashCombine(hash, info.tessEvalShader);
    hashCombine(hash, info.tessCtrlShader);
    return hash;
}

bool PipelineInfo::operator==(const PipelineInfo& other) const noexcept
{
    return type == other.type && sampleCount == other.sampleCount
           && enableMultisampling == other.enableMultisampling && enableColorBlend == other.enableColorBlend
           && topology == other.topology && rasterState == other.rasterState && depthState == other.depthState
           && vertexCompShader == other.vertexCompShader && fragmentShader == other.fragmentShader
           && geometryShader == other.geometryShader && tessEvalShader == other.tessEvalShader
           && tessCtrlShader == other.tessCtrlShader;
}

vk::PipelineCache loadCache(const char* path, const vk::Device device)
{
    vk::PipelineCacheCreateInfo createInfo{};
    try {
        File file(path);
        const auto data = file.read();
        file.close();
        createInfo.pInitialData = data.data();
        createInfo.initialDataSize = data.size();
        return device.createPipelineCache(createInfo);
    }
    catch (const PhysFsError& e) {
        SPDLOG_LOGGER_ERROR(
            spdlog::get("Rendering"),
            "Failed to load pipeline cache(this is normal on the first run): {}",
            e.what()
        );
        return device.createPipelineCache(createInfo);
    }
}

PipelineFactory::PipelineFactory(const Context& ctx, DescriptorLayoutManager* descriptorLayoutManager)
    : descriptorLayoutManager(descriptorLayoutManager), device(ctx.device)
{
    cache = loadCache(CACHE_PATH, ctx.device);
#ifdef SHADER_OUTPUT_PATH
    File::mount(SHADER_OUTPUT_PATH, SHADER_DIR);
#endif
    loadShaders(SHADER_DIR);
}

Pipeline PipelineFactory::getOrCreate(const PipelineInfo& info)
{
    const auto found = getPipeline(info);
    if (found.has_value())
        return found.value();
    return createPipeline(info);
}

std::optional<Pipeline> PipelineFactory::getPipeline(const PipelineInfo& info)
{
    std::shared_lock lock(mutex);
    const auto iter = pipelines.find(info);
    if (iter == pipelines.end())
        return std::nullopt;
    return iter->second;
}

PipelineFactory::~PipelineFactory()
{
    for (auto pipeline : std::ranges::views::values(pipelines)) {
        device.destroy(pipeline);
        device.destroy(pipeline.getLayout());
    }
    for (auto& [shader, refl] : std::ranges::views::values(shaders))
        device.destroy(shader);
    try {
        savePipelineCache();
    }
    catch (const std::runtime_error& e) {
        SPDLOG_LOGGER_ERROR(spdlog::get("Rendering"), "Failed to save pipeline cache: {}", e.what());
    }
    device.destroy(cache);
}

static const std::pair<vk::ShaderModule, spv_reflect::ShaderModule>& getShader(
    const std::string_view id,
    const StringMap<std::pair<vk::ShaderModule, spv_reflect::ShaderModule>>& shaders
)
{
    const auto iter = shaders.find(id);
    if (iter == shaders.end())
        throw std::out_of_range("Shader not found");
    return iter->second;
}

static uint32_t createStageInfos(
    vk::PipelineShaderStageCreateInfo* infos,
    const PipelineInfo& info,
    const StringMap<std::pair<vk::ShaderModule, spv_reflect::ShaderModule>>& shaders
)
{
    uint32_t count = 2;
    {
        const auto& shader = getShader(info.vertexCompShader, shaders);
        infos[0] = vk::PipelineShaderStageCreateInfo(
            {},
            vk::ShaderStageFlagBits::eVertex,
            shader.first,
            shader.second.GetSourceFile()
        );
    }
    {
        const auto& shader = getShader(info.fragmentShader, shaders);
        infos[1] = vk::PipelineShaderStageCreateInfo(
            {},
            vk::ShaderStageFlagBits::eFragment,
            shader.first,
            shader.second.GetSourceFile()
        );
    }
    if (!info.geometryShader.empty()) {
        const auto& shader = getShader(info.geometryShader, shaders);
        infos[count] = vk::PipelineShaderStageCreateInfo(
            {},
            vk::ShaderStageFlagBits::eGeometry,
            shader.first,
            shader.second.GetSourceFile()
        );
        count++;
    }
    if (!info.tessEvalShader.empty()) {
        const auto& shader = getShader(info.tessEvalShader, shaders);
        infos[count] = vk::PipelineShaderStageCreateInfo(
            {},
            vk::ShaderStageFlagBits::eTessellationEvaluation,
            shader.first,
            shader.second.GetSourceFile()
        );
        count++;
    }
    if (!info.tessCtrlShader.empty()) {
        const auto& shader = getShader(info.tessCtrlShader, shaders);
        infos[count] = vk::PipelineShaderStageCreateInfo(
            {},
            vk::ShaderStageFlagBits::eTessellationControl,
            shader.first,
            shader.second.GetSourceFile()
        );
        count++;
    }
    return count;
}

Pipeline PipelineFactory::createPipeline(const PipelineInfo& info)
{
    if (info.type == PipelineInfo::Type::COMPUTE) {
        const auto it = shaders.find(info.vertexCompShader);
        if (it == shaders.end())
            throw std::out_of_range(fmt::format("Shader {} not found", info.vertexCompShader));
        auto& [shader, reflect] = it->second;

        vk::ComputePipelineCreateInfo createInfo{};
        createInfo.stage.module = shader;
        createInfo.layout = createLayout(info);
        createInfo.stage.stage = vk::ShaderStageFlagBits::eCompute;
        createInfo.stage.pName = reflect.GetEntryPointName();

        auto [result, pipeline] = device.createComputePipeline(cache, createInfo);
        if (result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create compute pipeline");
        std::unique_lock lock(mutex);
        return pipelines[info] = Pipeline(pipeline, vk::PipelineBindPoint::eCompute, createInfo.layout);
    }

    vk::PipelineShaderStageCreateInfo stages[MAX_SHADER_COUNT];
    const uint32_t stageCount = createStageInfos(stages, info, shaders);

    constexpr std::array dynamicStates = {vk::DynamicState::eScissor, vk::DynamicState::eViewport};
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.dynamicStateCount = dynamicStates.size();
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.scissorCount = viewportState.viewportCount = 1;

    vk::PipelineMultisampleStateCreateInfo multisampleState{};
    multisampleState.rasterizationSamples = info.enableMultisampling ? info.sampleCount
                                                                     : vk::SampleCountFlagBits::e1;
    multisampleState.sampleShadingEnable = info.enableMultisampling;
    multisampleState.alphaToOneEnable = false;
    multisampleState.alphaToCoverageEnable = false;
    multisampleState.minSampleShading = 0.2f;

    vk::PipelineInputAssemblyStateCreateInfo inputAsm{};
    inputAsm.primitiveRestartEnable = false;
    inputAsm.topology = info.topology;

    vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.blendEnable = info.enableColorBlend;
    colorBlendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                               | vk::ColorComponentFlagBits::eB
                                               | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachmentState;
    colorBlend.logicOpEnable = false;

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    if (info.topology == vk::PrimitiveTopology::eTriangleList
        || info.topology == vk::PrimitiveTopology::eTriangleFan) {
        vertexInput.pVertexBindingDescriptions = MESH_VERTEX_INPUT_BINDING.data();
        vertexInput.vertexBindingDescriptionCount = MESH_VERTEX_INPUT_BINDING.size();
        vertexInput.pVertexAttributeDescriptions = MESH_VERTEX_ATTRIBUTES.data();
        vertexInput.vertexAttributeDescriptionCount = MESH_VERTEX_ATTRIBUTES.size();
    }
    else {
        crash("not yet implemented");// TODO
    }

    vk::GraphicsPipelineCreateInfo createInfo{};
    createInfo.renderPass = nullptr;
    createInfo.pDynamicState = &dynamicStateInfo;
    createInfo.pViewportState = &viewportState;
    createInfo.stageCount = stageCount;
    createInfo.pStages = stages;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pInputAssemblyState = &inputAsm;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDepthStencilState = &info.depthState;
    createInfo.pRasterizationState = &info.rasterState;
    createInfo.pVertexInputState = &vertexInput;
    createInfo.layout = createLayout(info);

    auto [result, pipeline] = device.createGraphicsPipeline(cache, createInfo);
    if (result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline");
    std::unique_lock lock(mutex);
    return pipelines[info] = Pipeline(pipeline, vk::PipelineBindPoint::eGraphics, createInfo.layout);
}

void PipelineFactory::savePipelineCache() const
{
    if (cache) {
        auto data = device.getPipelineCacheData(cache);
        File file(CACHE_PATH, File::Mode::WRITE);
        file.write(std::span(data));
        spdlog::get("Rendering")->info("Saved pipeline cache to disk");
    }
}

static void reflectPushConstantData(
    const spv_reflect::ShaderModule* reflect,
    SmallVector<vk::PushConstantRange, 6>& pushConstants
)
{
    uint32_t pushConstantCount;
    SpvReflectBlockVariable** vars = nullptr;
    if (reflect->EnumeratePushConstantBlocks(&pushConstantCount, vars) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("Failed to ennumerate push constants");
    vars = static_cast<SpvReflectBlockVariable**>(alloca(sizeof(SpvReflectBlockVariable*) * pushConstantCount)
    );
    if (reflect->EnumeratePushConstantBlocks(&pushConstantCount, vars) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("Failed to ennumerate push constants");

    for (uint32_t i = 0; i < pushConstantCount; i++) {
        vk::PushConstantRange& range = pushConstants.emplace();
        range.stageFlags = static_cast<vk::ShaderStageFlags>(reflect->GetShaderStage());
        range.size = vars[i]->size;
        range.offset = vars[i]->offset;
    }
}

static void reflectSetLayouts(
    const spv_reflect::ShaderModule* reflect,
    SmallVector<vk::DescriptorSetLayout>& setLayouts,
    DescriptorLayoutManager* descriptorLayoutManager
)
{
    uint32_t setCount;
    SpvReflectDescriptorSet** sets = nullptr;
    if (reflect->EnumerateDescriptorSets(&setCount, sets) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("Failed to reflect descriptor sets");
    sets = static_cast<SpvReflectDescriptorSet**>(alloca(sizeof(SpvReflectDescriptorSet*) * setCount));
    if (reflect->EnumerateDescriptorSets(&setCount, sets) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("Failed to reflect descriptor sets");

    for (uint32_t i = 0; i < setCount; i++) {
        SmallVector<vk::DescriptorSetLayoutBinding> bindings;
        bindings.reserve(sets[i]->binding_count);
        for (uint32_t j = 0; j < sets[i]->binding_count; j++) {
            const SpvReflectDescriptorBinding* binding = sets[i]->bindings[j];
            auto& b = bindings.emplace();
            b.binding = binding->binding;
            b.descriptorCount = binding->count;
            b.descriptorType = static_cast<vk::DescriptorType>(binding->descriptor_type);
            b.stageFlags = static_cast<vk::ShaderStageFlags>(reflect->GetShaderStage());
        }
        vk::DescriptorSetLayout setLayout = descriptorLayoutManager->createLayout(bindings);
        if (!setLayouts.contains(setLayout))
            setLayouts.pushBack(setLayout);
    }
}

vk::PipelineLayout PipelineFactory::createLayout(const PipelineInfo& info) const
{
    const spv_reflect::ShaderModule* reflectData[MAX_SHADER_COUNT];
    uint32_t reflectCount = 1;
    reflectData[0] = &getShader(info.vertexCompShader, shaders).second;
    if (!info.fragmentShader.empty())
        reflectData[reflectCount++] = &getShader(info.fragmentShader, shaders).second;
    if (!info.geometryShader.empty())
        reflectData[reflectCount++] = &getShader(info.geometryShader, shaders).second;
    if (!info.tessEvalShader.empty())
        reflectData[reflectCount++] = &getShader(info.tessEvalShader, shaders).second;
    if (!info.tessCtrlShader.empty())
        reflectData[reflectCount++] = &getShader(info.tessCtrlShader, shaders).second;

    vk::PipelineLayoutCreateInfo createInfo{};
    SmallVector<vk::PushConstantRange, 6> pushConstantRanges;
    SmallVector<vk::DescriptorSetLayout> setLayouts;
    for (uint32_t i = 0; i < reflectCount; i++) {
        reflectPushConstantData(reflectData[i], pushConstantRanges);
        reflectSetLayouts(reflectData[i], setLayouts, descriptorLayoutManager);
    }
    createInfo.pPushConstantRanges = pushConstantRanges.data();
    createInfo.pushConstantRangeCount = pushConstantRanges.size();
    createInfo.pSetLayouts = setLayouts.data();
    createInfo.setLayoutCount = setLayouts.size();

    return device.createPipelineLayout(createInfo);
}

void PipelineFactory::loadShaders(const char* dir)
{
    const auto logger = spdlog::get("Rendering");
    char** ls = PHYSFS_enumerateFiles(dir);

    std::unique_lock lock(mutex);
    for (char** ptr = ls; *ptr; ptr++) {
        const char* ext = strrchr(*ptr, '.');
        if (ext == nullptr || strcmp(ext, ".spv") != 0) {
            SPDLOG_LOGGER_TRACE(logger, "File {} is not a SPIR-V file", *ptr);
            continue;
        }

        try {
            std::string path = *ptr;
            path = path.substr(0, path.find(".spv"));
            auto& shaderData = shaders[path];
            path = dir;
            path += "/";
            path += *ptr;
            File file(path);
            auto data = file.read();

            shaderData.second = spv_reflect::ShaderModule(data);
            if (shaderData.second.GetResult() != SPV_REFLECT_RESULT_SUCCESS)
                throw std::runtime_error("SPIR-V shader reflection failed");
            vk::ShaderModuleCreateInfo createInfo{};
            createInfo.pCode = shaderData.second.GetCode();
            createInfo.codeSize = shaderData.second.GetCodeSize();
            shaderData.first = device.createShaderModule(createInfo);
            logger->info("Loaded shader file \"{}\"", *ptr);
        }
        catch (const std::exception& e) {
            SPDLOG_LOGGER_ERROR(logger, "Failed to load shader file \"{}\", error: {}", *ptr, e.what());
        }
    }
    PHYSFS_freeList(ls);
}

}// namespace dragonfire::vulkan