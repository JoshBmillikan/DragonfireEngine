//
// Created by josh on 6/29/24.
//

#pragma once
#include "chunk.h"
#include "voxel.h"
#include <FastNoise/FastNoise.h>
#include <glm/vec3.hpp>

namespace dragonfire::voxel {

class TerrainGenerator {
public:
    const uint64_t seed;

    TerrainGenerator(uint64_t seed);
    TerrainGenerator(uint64_t seed, int octaves, float gain, float weight, float lacunarity);

    Chunk genChunk(glm::i32vec3 corner);

private:
    FastNoise::SmartNode<> noise;
};

class Terrain {
    std::unordered_map<glm::i64vec3, Chunk> chunks;
};

}// namespace dragonfire::voxel
