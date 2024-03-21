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
    // flip y for compatibility with the Vulkan coordinate system instead of OpenGL
    perspective[1][1] *= -1;
    orthograhpic[1][1] *= -1;
}

void Camera::lookAt(const glm::vec3 target)
{
    const glm::mat4 look = glm::lookAtRH(position, target, UP);
    position = look[3];
}

static glm::vec4 normalizePlane(const glm::vec4 p)
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

glm::mat4 Camera::getViewMatrix() const noexcept
{
    glm::vec3 direction;
    direction.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
    direction.y = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
    direction.z = sinf(glm::radians(pitch));
    return glm::lookAtRH(position, direction, UP) ;
}

}// namespace dragonfire