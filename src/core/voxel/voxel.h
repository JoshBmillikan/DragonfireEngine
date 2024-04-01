//
// Created by josh on 3/29/24.
//

#pragma once
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <shared_mutex>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace dragonfire {

struct Voxel {
    static constexpr uint16_t ID_SIZE = 14;
    static constexpr uint16_t MAX_VOXEL_COUNT = 1 << ID_SIZE;
    uint16_t id : ID_SIZE;
    uint16_t transparent : 1;
};

struct VoxelType {
    std::string name;
    float hardness;
};

class VoxelRegistry {
public:
    void loadVoxelTypes(const char* jsonPath);
    void loadVoxelTypes(std::span<const char*> filePaths);

    std::tuple<const VoxelType*, std::shared_lock<std::shared_mutex>> get(const Voxel v) const
    {
        std::shared_lock l(mutex);
        return std::make_tuple(&voxels[v.id], std::move(l));
    }

    std::tuple<const VoxelType*, std::shared_lock<std::shared_mutex>> get(const uint16_t id) const
    {
        std::shared_lock l(mutex);
        return std::make_tuple(&voxels[id], std::move(l));
    }

    std::tuple<const VoxelType*, std::shared_lock<std::shared_mutex>> operator[](const Voxel v) const
    {
        return get(v);
    }

    static VoxelRegistry& get() { return INSTANCE; }

private:
    static VoxelRegistry INSTANCE;
    mutable std::shared_mutex mutex;
    std::vector<VoxelType> voxels;
    ankerl::unordered_dense::map<std::string, uint16_t> idMap;
};

}// namespace dragonfire
