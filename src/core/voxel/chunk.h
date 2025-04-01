//
// Created by josh on 7/4/24.
//

#pragma once
#include "voxel.h"
#include <cstdint>
#include <glm/vec3.hpp>

namespace dragonfire::voxel {
class Chunk {
public:
    static constexpr std::size_t CHUNK_DIM = 32;
    static constexpr std::size_t CHUNK_SIZE = CHUNK_DIM * CHUNK_DIM * CHUNK_DIM;

    Voxel operator [](const glm::ivec3 index) const {return voxels[index.x][index.y][index.z];}

private:
    Voxel voxels[CHUNK_DIM][CHUNK_DIM][CHUNK_DIM]{};
};

}// namespace dragonfire::voxel
