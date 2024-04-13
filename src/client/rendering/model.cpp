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
    primitives.emplace_back(mesh, std::shared_ptr<Material>(material), bounds, transform);
}

void Model::writeDrawData(Drawables& drawables, const Transform& baseTransform) const
{
    const glm::mat4 base = baseTransform.toMatrix();
    for (const auto& primitive : primitives) {
        assert(primitive.material);
        drawables[primitive.material.get()]
            .emplace_back(primitive.mesh, base * primitive.transform, primitive.bounds);
    }
}

}// namespace dragonfire