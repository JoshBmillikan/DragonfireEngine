//
// Created by josh on 6/30/24.
//
#include "voxel.h"
#include <catch.hpp>

using namespace dragonfire::voxel;

TEST_CASE("loadJsonString")
{
    VoxelRegistry registry;

    const char* json = R"(
    {"name":"test", "hardness": 2.0}
)";
    REQUIRE_NOTHROW(registry.loadJsonString(json));
}