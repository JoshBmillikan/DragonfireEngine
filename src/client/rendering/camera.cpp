//
// Created by josh on 1/24/24.
//

#include "camera.h"


#include <glm/ext/matrix_clip_space.hpp>

namespace dragonfire {
Camera::Camera(const float fov, const float width, const float height, const float zNear, const float zFar)
    : zNear(zNear), zFar(zFar)
{
    perspective = glm::perspectiveFovRH_ZO(fov, width, height, zNear, zFar);
    orthograhpic = glm::orthoRH_ZO(0.0f, width, 0.0f, height, zNear, zFar);
}

void Camera::lookAt(const glm::vec3 target)
{
    glm::mat4 look = glm::lookAtRH(position, target, {0, 0, 1});
    position = glm::vec3(look[3]);
    rotation = glm::toQuat(look);
}

static glm::vec4 normalizePlane(glm::vec4 p)
{
    return p / glm::length(glm::vec3(p));
}

std::pair<glm::vec4, glm::vec4> Camera::getFrustumPlanes() const
{
    glm::mat4 pt = glm::transpose(perspective);
    return {
        normalizePlane(pt[3] + pt[0]),
        normalizePlane(pt[3] + pt[1]),
    };
}
}// namespace dragonfire