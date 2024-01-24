//
// Created by josh on 1/24/24.
//

#include "camera.h"


#include <glm/ext/matrix_clip_space.hpp>

namespace dragonfire {
Camera::Camera(const float fov, const float width, const float height, const float zNear, const float zFar)
{
    perspective = glm::perspectiveFovRH_ZO(fov, width, height, zNear, zFar);
    orthograhpic = glm::orthoRH_ZO(0.0f, width, 0.0f, height, zNear, zFar);
}
}// namespace dragonfire