//
// Created by josh on 5/19/22.
//

#pragma once
#include <BaseMesh.h>
#include "Allocation.h"

namespace dragonfire::rendering {
class Mesh : public BaseMesh {
    Buffer buffer;
public:
    int32_t vertexOffset;
    vk::Buffer getBuffer() {
        return buffer;
    }

    void bind(vk::CommandBuffer cmd) const noexcept ;
};
}