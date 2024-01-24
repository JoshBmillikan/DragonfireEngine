//
// Created by josh on 1/15/24.
//

#pragma once
#include "allocation.h"
#include "context.h"
#include "descriptor_set.h"
#include "mesh.h"
#include "swapchain.h"
#include "pipeline.h"
#include <client/rendering/base_renderer.h>

namespace dragonfire::vulkan {

class VulkanRenderer final : public BaseRenderer {
public:
    explicit VulkanRenderer(bool enableValidation);
    ~VulkanRenderer() override;

private:
    Context context;
    GpuAllocator allocator;
    Swapchain swapchain;
    std::unique_ptr<MeshRegistry> meshRegistry;
    DescriptorLayoutManager descriptorLayoutManager;
    std::unique_ptr<PipelineFactory> pipelineFactory;
};

}// namespace dragonfire::vulkan