//
// Created by josh on 5/30/23.
//

#pragma once
#include <cmath>
#include <glm/glm.hpp>

namespace dragonfire {

inline constexpr glm::vec3 midpoint(glm::vec3 a, glm::vec3 b)
{
    return glm::vec3((a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f, (a.z + b.z) / 2.0f);
}

inline constexpr float lerp(float a, float b, float t)
{
    return std::fma(t, b, std::fma(-t, a, a));
}

inline constexpr float getMatrixScaleFactor(const glm::mat4& m)
{
    return std::sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
}

}   // namespace dragonfire