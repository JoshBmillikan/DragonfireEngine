//
// Created by josh on 5/21/22.
//

#include "Shader.h"
#include <FileLocator.h>
#include <Util.h>
#include <fstream>
#include <nlohmann/json.hpp>

namespace dragonfire::rendering {
namespace fs = std::filesystem;

/// loads the pipeline cache from the disk, or creates
/// a new one if no cache currently exists
static vk::PipelineCache loadCache(vk::Device device);
static vk::UniqueShaderModule loadShader(const fs::path& path, vk::Device device);

Shader::Shader(std::string_view info, std::vector<Module>&& modules) : modules(std::move(modules)) {
    nlohmann::json pipelineInfo = info;
}

Shader::Library::Library(vk::Device device) : device(device) {
    cache = loadCache(device);
    auto path = Service::get<FileLocator>().assetDir;
    path.append("shaders");
    for (auto& entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".spv")
            loadedShaders[entry.path().stem()] = loadShader(entry.path(), device);
    }
}

vk::ShaderModule& Shader::Library::getShader(const std::string& shaderName) { return loadedShaders.at(shaderName).get(); }

static fs::path getCachePath();

Shader::Library::~Library() noexcept {
    const auto path = getCachePath();
    // todo write cache back to disk
    device.destroy(cache);
}

static vk::PipelineCache loadCache(vk::Device device) {
    const auto path = getCachePath();
    if (exists(path)) {
        auto data = readFileToBytes(path);
        vk::PipelineCacheCreateInfo createInfo{
                .initialDataSize = data.size(),
                .pInitialData = data.data(),
        };
        return device.createPipelineCache(createInfo);
    }
    else {
        return device.createPipelineCache(vk::PipelineCacheCreateInfo{});
    }
}

static fs::path getCachePath() {
    auto& locator = Service::get<FileLocator>();
    auto path = locator.dataDir;
    path.append("ShaderCache");
    return path;
}

static vk::UniqueShaderModule loadShader(const fs::path& path, vk::Device device) {
    std::ifstream stream(path, std::ios::in | std::ios::binary | std::ios::ate);
    auto ext = path.stem().extension().string();

    // TODO handle endianness

}

}   // namespace dragonfire::rendering
