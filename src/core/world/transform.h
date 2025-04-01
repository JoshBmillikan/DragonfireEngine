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

    ~Transform() = default;
    Transform(
        const glm::vec3 position = {},
        const glm::vec3 scale = {1.0f, 1.0f, 1.0f},
        const glm::quat rotation = glm::identity<glm::quat>()
    )
        : position(position), scale(scale), rotation(rotation)
    {
    }

    [[nodiscard]] glm::mat4 toMatrix() const
    {
        return glm::translate(glm::identity<glm::mat4>(), position) * glm::toMat4(rotation)
               * glm::scale(glm::identity<glm::mat4>(), scale);
    }

    Transform(const Transform& other) = default;
    Transform(Transform&& other) noexcept = default;
    Transform& operator=(const Transform& other) = default;
    Transform& operator=(Transform&& other) noexcept = default;
};

}// namespace dragonfire