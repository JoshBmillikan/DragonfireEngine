//
// Created by josh on 6/29/24.
//

#include "voxel.h"
#include "core/file.h"
#include <simdjson.h>

using namespace simdjson;

template<>
simdjson_inline simdjson_result<dragonfire::voxel::VoxelType> ondemand::value::get() noexcept
{
    ondemand::object obj;
    auto error = get_object().get(obj);
    if (error)
        return error;

    dragonfire::voxel::VoxelType voxel;

    for (auto field : obj) {
        ondemand::raw_json_string key;
        error = field.key().get(key);
        if (error)
            return error;
        if (key == "name")
            error = field.value().get_string(voxel.name);
        else if (key == "displayName")
            error = field.value().get_string(voxel.displayName);
        else if (key == "hardness") {
            double d;
            error = field.value().get_double().get(d);
            voxel.hardness = float(d);
        }
        if (error)
            return error;
    }
    if (voxel.displayName.empty())
        voxel.displayName = voxel.name;
    return voxel;
}

namespace dragonfire::voxel {

void VoxelRegistry::loadJsonFiles(const std::span<const char*> paths)
{
    ondemand::parser parser;
    for (const char* path : paths) {
        File file(path);
        std::string jsonString = file.readString();
        file.close();
        jsonString.resize(jsonString.size() + SIMDJSON_PADDING);
        const auto view = padded_string_view(jsonString);
        auto doc = parser.iterate(view).value();
        if (doc.type() == ondemand::json_type::array) {
            auto array = doc.get_array().value();
            for (auto element : array) {
                auto val = element.value();
                const uint32_t index = voxelTypes.size();
                const auto& voxel = voxelTypes.emplace_back(val);
                voxelIdMap[voxel.name] = index;
            }
        }
        else {
            auto val = doc.get_value().value();
            const uint32_t index = voxelTypes.size();
            const auto& voxel = voxelTypes.emplace_back(val);
            voxelIdMap[voxel.name] = index;
        }
    }
}

void VoxelRegistry::loadJsonString(const std::string_view string)
{
    ondemand::parser parser;
    const padded_string json = string;
    auto doc = parser.iterate(json).value();
    if (doc.type() == ondemand::json_type::array) {
        auto array = doc.get_array().value();
        for (auto element : array) {
            auto val = element.value();
            voxelTypes.emplace_back(val);
        }
    }
    else {
        auto val = doc.get_value().value();
        voxelTypes.emplace_back(val);
    }
}

}// namespace dragonfire::voxel
