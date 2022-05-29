//
// Created by josh on 5/19/22.
//

#include "Material.h"
#include <FileLocator.h>
#include <nlohmann/json.hpp>
#include <spirv_reflect.h>
#include <optional>
#include <alloca.h>

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

struct SetLayoutData {
    uint32_t setNum;
    vk::DescriptorSetLayoutCreateInfo info;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

static SetLayoutData getLayoutData(spv_reflect::ShaderModule& module) {
    uint32_t setCount;
    SpvReflectDescriptorSet** sets = nullptr;
    if (module.EnumerateDescriptorSets(&setCount, sets) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("Failed to reflect descriptor set count");
    sets = (SpvReflectDescriptorSet**) alloca(sizeof(SpvReflectDescriptorSet*) * setCount);
    if (module.EnumerateDescriptorSets(&setCount, sets) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("Failed to reflect descriptor sets");
    for (uint32_t i=0;i<setCount;i++) {

    }
}

vk::Pipeline MaterialFactory::createPipeline(const std::string& materialName) {
    SQLite::Statement stmt(
            db,
            "SELECT spv, spv_size FROM material WHERE name=? "
            "INNER JOIN shader_material ON shader_material.material=material.id "
            "INNER JOIN shader ON shader.id=shader_material.shader"
    );
    stmt.bindNoCopy(1, materialName);
    vk::ShaderModule vertex, fragment, geom, tessEval, tessCtrl;
    vertex = fragment = geom = tessEval = tessCtrl = nullptr;
    std::array<std::optional<SetLayoutData>, 5> setData{};
    while (stmt.executeStep()) {
        auto data = reinterpret_cast<const uint32_t*>(stmt.getColumn("spv").getBlob());
        auto size = stmt.getColumn("spv_size").getInt64();
        spv_reflect::ShaderModule reflectModule(size, data);
        auto stage = reflectModule.GetShaderStage();
        vk::ShaderModuleCreateInfo createInfo {
                .flags = static_cast<vk::ShaderModuleCreateFlagBits>(stage),
                .codeSize = static_cast<size_t>(size),
        };
        switch (stage) {
            case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
                if(vertex)
                    throw std::runtime_error("Duplicate vertex shader stages");
                setData[0] = getLayoutData(reflectModule);
                vertex = device.createShaderModule(createInfo);
                break;
            case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                if(tessCtrl)
                    throw std::runtime_error("Duplicate tesselation control shader stages");
                setData[4] = getLayoutData(reflectModule);
                tessCtrl = device.createShaderModule(createInfo);
                break;
            case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                if(tessEval)
                    throw std::runtime_error("Duplicate tesselation evaluation shader stages");
                setData[3] = getLayoutData(reflectModule);
                tessEval = device.createShaderModule(createInfo);
                break;
            case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
                if(geom)
                    throw std::runtime_error("Duplicate geometry shader stages");
                setData[2] = getLayoutData(reflectModule);
                geom = device.createShaderModule(createInfo);
                break;
            case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
                if(fragment)
                    throw std::runtime_error("Duplicate fragment shader stages");
                setData[1] = getLayoutData(reflectModule);
                fragment = device.createShaderModule(createInfo);
                break;
            default: throw std::runtime_error("Unexpected shader stage for graphics pipeline");
        }
    }

    vk::PipelineRenderingCreateInfo renderingInfo {
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &format,
            // todo depth format
    };

    vk::GraphicsPipelineCreateInfo createInfo {
            .pNext = &renderingInfo,
            .renderPass = nullptr,
        //todo
    };

    auto [result, pipeline] = device.createGraphicsPipeline(cache, createInfo);
    vk::resultCheck(result, "Failed to create graphics pipeline");
    return pipeline;
}
