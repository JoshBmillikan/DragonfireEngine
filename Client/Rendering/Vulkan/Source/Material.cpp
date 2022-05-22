//
// Created by josh on 5/19/22.
//

#include "Material.h"
#include <FileLocator.h>
#include <nlohmann/json.hpp>

using namespace dragonfire::rendering;

MaterialFactory::MaterialFactory(vk::Device device) : device(device) {
    const auto& locator = Service::get<FileLocator>();
    auto file = locator.dataDir;
    file.append("ShaderCache");
    if (!exists(file))
        cache = device.createPipelineCache(vk::PipelineCacheCreateInfo{});
    else {
        std::ifstream stream(file, std::ios::in | std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        vk::PipelineCacheCreateInfo createInfo{
                .initialDataSize = data.size(),
                .pInitialData = data.data(),
        };
        cache = device.createPipelineCache(createInfo);
    }
}

MaterialFactory::~MaterialFactory() {
    const auto& locator = Service::get<FileLocator>();
    auto file = locator.dataDir;
    file.append("ShaderCache");
    auto data = device.getPipelineCacheData(cache);
    std::ofstream out(file, std::ios::out | std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), (ssize_t) data.size());
    if (out.bad())
        spdlog::error("Failed to save pipeline cache");
    else
        out.flush();
    device.destroy(cache);
}

Material* MaterialFactory::createMaterial(std::ifstream& stream, const std::string& materialName) { return nullptr; }
