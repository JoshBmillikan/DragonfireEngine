//
// Created by josh on 1/15/24.
//

#pragma once
#include <client/rendering/base_renderer.h>
#include "context.h"
#include "allocation.h"
#include "swapchain.h"

namespace dragonfire::vulkan {

class VulkanRenderer final : public BaseRenderer {
public:
    explicit VulkanRenderer(bool enableValidation);
    ~VulkanRenderer() override;

private:
    Context context;
    GpuAllocator allocator;
    Swapchain swapchain;
};

}// namespace dragonfire