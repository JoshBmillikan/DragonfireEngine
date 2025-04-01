//
// Created by josh on 6/29/24.
//

#pragma once
#include "core/utility/string_hash.h"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace dragonfire::voxel {

struct Voxel {
    static constexpr size_t ID_BIT_COUNT = 15;
    static constexpr size_t MAX_VOXEL_COUNT = 1 << ID_BIT_COUNT;
    uint16_t id : ID_BIT_COUNT;
    uint16_t transparent : 1;
};

struct VoxelType {
    std::string name, displayName;
    float hardness = 1.0;
};

class VoxelRegistry {
public:
    const VoxelType& operator[](const Voxel voxel) const { return voxelTypes[voxel.id]; }

    const VoxelType& operator[](const std::string_view id) const
    {
        const uint32_t index = voxelIdMap.at(id);
        return voxelTypes[index];
    }

    template<typename... Args>
    void loadJsonFiles(Args... args)
    {
        std::array array{args...};
        loadJsonFiles(array);
    }

    void loadJsonFiles(std::span<const char*> paths);
    void loadJsonString(std::string_view string);

private:
    std::vector<VoxelType> voxelTypes;
    StringFlatMap<uint32_t> voxelIdMap;
};

}// namespace dragonfire::voxel
