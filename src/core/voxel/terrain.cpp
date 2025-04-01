//
// Created by josh on 6/29/24.
//

#include "terrain.h"

namespace dragonfire::voxel {
TerrainGenerator::TerrainGenerator(const uint64_t seed) : seed(seed)
{
    noise = FastNoise::New<FastNoise::Simplex>();
}

TerrainGenerator::TerrainGenerator(
    const uint64_t seed,
    const int octaves,
    const float gain,
    const float weight,
    const float lacunarity
)
    : seed(seed)
{
    const auto simplex = FastNoise::New<FastNoise::Simplex>();
    const auto fbm = FastNoise::New<FastNoise::FractalFBm>();
    fbm->SetSource(simplex);
    fbm->SetOctaveCount(octaves);
    fbm->SetGain(gain);
    fbm->SetLacunarity(lacunarity);
    fbm->SetWeightedStrength(weight);
    noise = fbm;
}

Chunk TerrainGenerator::genChunk(glm::i32vec3 corner)
{
    float buffer[Chunk::CHUNK_SIZE];
    noise->GenUniformGrid2D(buffer, corner.x, corner.y, Chunk::CHUNK_DIM, Chunk::CHUNK_DIM, 1.0, seed);
}
}// namespace dragonfire::voxel
