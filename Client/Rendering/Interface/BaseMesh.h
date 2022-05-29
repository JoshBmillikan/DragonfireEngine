//
// Created by josh on 5/19/22.
//

#pragma once
#include <Asset.h>

namespace dragonfire::rendering {
struct Vertex {
    glm::vec3 positions;
};
struct BaseMesh : Asset {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    BaseMesh(std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices) : vertices(vertices), indices(indices) {}
};
}