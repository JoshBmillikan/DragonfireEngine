//
// Created by josh on 1/24/24.
//

#pragma once
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace dragonfire {

struct Camera {
    glm::mat4 perspective{};
    glm::mat4 orthograhpic{};
    glm::mat4 transform = glm::identity<glm::mat4>();

    Camera(float fov, float width, float height, float zNear, float zFar);
};

} // dragonfire
