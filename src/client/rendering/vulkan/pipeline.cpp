//
// Created by josh on 1/19/24.
//

#include "pipeline.h"
#include "context.h"
#include "core/file.h"
#include <physfs.h>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_hash.hpp>

namespace dragonfire::vulkan {

size_t PipelineInfo::hash(const PipelineInfo& info) noexcept {}

bool PipelineInfo::operator==(const PipelineInfo& other) const noexcept {}

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

PipelineFactory::PipelineFactory(const Context& ctx) : device(ctx.device)
{
    cache = loadCache(CACHE_PATH, ctx.device);
#ifdef SHADER_OUTPUT_PATH
    File::mount(SHADER_OUTPUT_PATH, SHADER_DIR);
#endif
    loadShaders(SHADER_DIR);
}

Pipeline PipelineFactory::getOrCreate(const PipelineInfo& info)
{
    auto found = getPipeline(info);
    if (found.has_value())
        return found.value();
    return createPipeline(info);
}

std::optional<Pipeline> PipelineFactory::getPipeline(const PipelineInfo& info)
{
    std::shared_lock lock(mutex);
    auto iter = pipelines.find(info);
    if (iter == pipelines.end())
        return std::nullopt;
    return iter->second;
}

PipelineFactory::~PipelineFactory()
{
    for (auto pipeline : std::ranges::views::values(pipelines))
        device.destroy(pipeline);
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

Pipeline PipelineFactory::createPipeline(const PipelineInfo& info)
{
    if (info.type == PipelineInfo::Type::COMPUTE) {
        vk::ComputePipelineCreateInfo createInfo{};

        auto [result, pipeline] = device.createComputePipeline(cache, createInfo);
        if (result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create compute pipeline");
        std::unique_lock lock(mutex);
        return pipelines[info] = Pipeline(pipeline, vk::PipelineBindPoint::eCompute, nullptr);
    }


    std::array dynamicStates = {vk::DynamicState::eScissor, vk::DynamicState::eViewport};
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.dynamicStateCount = dynamicStates.size();
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.scissorCount = viewportState.viewportCount = 1;

    vk::GraphicsPipelineCreateInfo createInfo{};
    createInfo.renderPass = nullptr;
    createInfo.pDynamicState = &dynamicStateInfo;
    createInfo.pViewportState = &viewportState;
    //TODO

    auto [result, pipeline] = device.createGraphicsPipeline(cache, createInfo);
    if (result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline");
    std::unique_lock lock(mutex);
    return pipelines[info] = Pipeline(pipeline, vk::PipelineBindPoint::eGraphics, nullptr);
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
        }
        catch (const std::exception& e) {
            SPDLOG_LOGGER_ERROR(logger, "Failed to load shader file \"{}\", error: {}", *ptr, e.what());
        }
    }
    PHYSFS_freeList(ls);
}

}// namespace dragonfire::vulkan