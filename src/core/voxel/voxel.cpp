//
// Created by josh on 3/29/24.
//

#include "voxel.h"
#include "core/file.h"
#include <simdjson.h>
#include <spdlog/spdlog.h>

namespace dragonfire {

VoxelRegistry VoxelRegistry::INSTANCE;

static void parseVoxelsFromJson(
    simdjson::ondemand::parser& parser,
    const simdjson::padded_string_view& str,
    std::vector<VoxelType>& voxels,
    ankerl::unordered_dense::map<std::string, uint16_t>& idMap
)
{
    simdjson::ondemand::document doc = parser.iterate(str);
    for (simdjson::ondemand::value value : doc) {
        const uint16_t id = voxels.size();
        auto& voxel = voxels.emplace_back();
        voxel.name = std::string(value["name"].get_string().value());
        idMap[voxel.name] = id;
        voxel.hardness = static_cast<float>(value["hardness"].get_double());
    }
}

void VoxelRegistry::loadVoxelTypes(const char* jsonPath)
{
    std::array a{jsonPath};
    loadVoxelTypes(a);
}

void VoxelRegistry::loadVoxelTypes(const std::span<const char*> filePaths)
{
    simdjson::ondemand::parser parser;
    std::string baseStr;
    baseStr.reserve(1024);
    std::unique_lock lock(mutex);
    for (const char* path : filePaths) {
        try {
            File file(path);
            baseStr = file.readString();
            file.close();
            baseStr.resize(baseStr.size() + simdjson::SIMDJSON_PADDING);
            const simdjson::padded_string_view str(baseStr);
            parseVoxelsFromJson(parser, str, voxels, idMap);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("Error in parsing voxel type json file \"{}\", error: {}", path, e.what());
        }
    }
}
}// namespace dragonfire