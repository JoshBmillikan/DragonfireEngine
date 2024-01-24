//
// Created by josh on 1/24/24.
//

#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace dragonfire {

struct Transform {
    glm::vec3 position;
    glm::vec3 scale;
    glm::quat rotation;
};

}// namespace dragonfire