//
// Created by josh on 1/20/24.
//

#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace dragonfire {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

}// namespace dragonfire