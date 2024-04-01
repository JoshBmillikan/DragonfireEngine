//
// Created by josh on 3/30/24.
//

#pragma once
#include "voxel.h"
#include <cstdint>
#include <vector>

namespace dragonfire {

class Octree {
public:

private:
    using VoxelIndexType = uint32_t;
    static constexpr VoxelIndexType NULL_VOXEL = 0;

    struct Node {
        VoxelType* voxel = nullptr;
        std::array<VoxelIndexType, 8> children{};
    };
    Node root;

    std::vector<Node> nodes;

    [[nodiscard]] const Node& getNode(VoxelIndexType index) const noexcept;
};

}// namespace dragonfire