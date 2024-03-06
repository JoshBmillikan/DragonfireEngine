//
// Created by josh on 1/24/24.
//

#pragma once
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace dragonfire {

struct Camera {
    glm::mat4 perspective{};
    glm::mat4 orthograhpic{};
    glm::vec3 position{};
    glm::quat rotation = glm::identity<glm::quat>();

    Camera(float fov, float width, float height, float zNear, float zFar);
    Camera() = default;

    void lookAt(glm::vec3 target);

    [[nodiscard]] std::pair<glm::vec4, glm::vec4> getFrustumPlanes() const;

    [[nodiscard]] float getZNear() const { return zNear; }

    [[nodiscard]] float getZFar() const { return zFar; }

    [[nodiscard]] glm::mat4 getViewMatrix() const noexcept
    {
        return glm::translate(glm::identity<glm::mat4>(), position) * toMat4(rotation);
    }

private:
    float zNear = 0.0f, zFar = 0.0f;
};

}// namespace dragonfire
