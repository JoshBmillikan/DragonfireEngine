//
// Created by josh on 1/24/24.
//

#pragma once
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace dragonfire {

class Camera {
public:
    static constexpr glm::vec3 UP = {0.0f, 0.0f, 1.0f};
    glm::mat4 perspective{};
    glm::mat4 orthograhpic{};
    glm::vec3 position{};

    Camera(float fov, float width, float height, float zNear, float zFar);
    Camera() = default;

    void lookAt(glm::vec3 target);

    [[nodiscard]] std::pair<glm::vec4, glm::vec4> getFrustumPlanes() const;

    [[nodiscard]] float getZNear() const { return zNear; }

    [[nodiscard]] float getZFar() const { return zFar; }

    [[nodiscard]] glm::mat4 getViewMatrix() const noexcept;

private:
    float zNear = 0.0f, zFar = 0.0f, pitch = 0.0f, yaw = -90.0f ;
};

}// namespace dragonfire
