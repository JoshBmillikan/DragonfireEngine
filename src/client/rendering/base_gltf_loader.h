//
// Created by josh on 1/29/24.
//

#pragma once
#include "core/utility/small_vector.h"
#include "material.h"
#include <fastgltf/parser.hpp>

namespace dragonfire {
using Mesh = std::uintptr_t;

class GltfLoader {
public:
    virtual ~GltfLoader() = default;
    virtual SmallVector<std::pair<Mesh, Material>> load(const char* path) = 0;

protected:
    fastgltf::Parser parser;
    fastgltf::Asset asset;
    bool optimizeMeshes = true;

};
}// namespace dragonfire