//
// Created by josh on 1/15/24.
//

#pragma once
#include <client/rendering/base_renderer.h>
#include "context.h"

namespace dragonfire::vulkan {

class VulkanRenderer final : public BaseRenderer {
public:

private:
    Context context;
};

}// namespace dragonfire