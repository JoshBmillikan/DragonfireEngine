//
// Created by josh on 2/7/24.
//

#pragma once
#include "core/utility/small_vector.h"
#include "material.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace dragonfire {
using Mesh = std::uintptr_t;

class Model {
    struct Primitive {
        Mesh mesh = 0;
        Material* material = nullptr;
        glm::mat4 transform = glm::identity<glm::mat4>();
    };

    SmallVector<Primitive, 2> primitives;
};
}// namespace dragonfire
