//
// Created by josh on 2/7/24.
//

#include "model.h"

namespace dragonfire {

void Model::addPrimitive(
    const Mesh mesh,
    Material* material,
    const glm::vec4 bounds,
    const glm::mat4& transform
)
{
    primitives.emplace(mesh, material, bounds, transform);
}

Model::~Model()
{
    for (auto& prim : primitives) {
        delete prim.material;
    }
}
}// namespace dragonfire