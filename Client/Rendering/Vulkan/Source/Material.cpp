//
// Created by josh on 5/19/22.
//

#include "Material.h"
#include <FileLocator.h>
#include <nlohmann/json.hpp>

using namespace dragonfire::rendering;
using namespace dragonfire;

void Material::bind(vk::CommandBuffer cmd) const noexcept {

}

std::shared_ptr<Material> MaterialFactory::createMaterial(const std::string& materialName) {
    try {
        auto weak = materials.at(materialName);
        if (!weak.expired())
            return weak.lock();
    }
    catch (const std::out_of_range& e) {
    }
    SQLite::Statement stmt(
            db,
            "SELECT spv FROM material WHERE name=? "
            "INNER JOIN shader_material ON shader_material.material=material.id "
            "INNER JOIN shader ON shader.id=shader_material.shader"
    );
    stmt.bindNoCopy(1, materialName);
    stmt.executeStep();
    // todo
    return nullptr;
}

static SQLite::Database connect() {
    const auto& locator = Service::get<FileLocator>();
    auto dbPath = locator.assetDir;
    dbPath.append("materials.db");
    return {dbPath, SQLite::OPEN_READONLY | SQLite::OPEN_NOMUTEX | SQLite::OPEN_NOFOLLOW};
}

MaterialFactory::MaterialFactory(vk::Device device) : device(device), db(connect()) {
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

MaterialFactory::~MaterialFactory() noexcept {
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
