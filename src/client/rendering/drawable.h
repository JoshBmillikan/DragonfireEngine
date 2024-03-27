//
// Created by josh on 3/27/24.
//

#pragma once
#include "core/utility/temp_containers.h"
#include "core/world/transform.h"
#include "material.h"
#include <ankerl/unordered_dense.h>
#include <glm/glm.hpp>
using Mesh = std::uintptr_t;

namespace dragonfire {

struct Drawable {
    struct Draw {
        Mesh mesh;
        glm::mat4 transform;
        glm::vec4 bounds;
    };
    using Drawables = ankerl::unordered_dense::map<const Material*, TempVec<Drawable::Draw>>;
    virtual ~Drawable() = default;
    virtual void writeDrawData(Drawables& drawables, const Transform& baseTransform) const = 0;
};

} // dragonfire
