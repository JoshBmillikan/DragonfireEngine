//
// Created by josh on 1/15/24.
//

#pragma once
#include "allocation.h"
#include "context.h"
#include "descriptor_set.h"
#include "mesh.h"
#include "pipeline.h"
#include "swapchain.h"

#include <client/rendering/base_renderer.h>
#include <condition_variable>
#include <thread>

namespace dragonfire::vulkan {

class VulkanRenderer final : public BaseRenderer {
public:
    explicit VulkanRenderer(bool enableValidation);
    ~VulkanRenderer() override;

    struct Frame {
        vk::Semaphore renderingSemaphore, presentSemaphore;
    };

    static constexpr int FRAMES_IN_FLIGHT = 2;

private:
    Context context;
    GpuAllocator allocator;
    Swapchain swapchain;
    std::unique_ptr<MeshRegistry> meshRegistry;
    DescriptorLayoutManager descriptorLayoutManager;
    std::unique_ptr<PipelineFactory> pipelineFactory;
    std::array<Frame, FRAMES_IN_FLIGHT> frames;

    struct {
        std::mutex mutex;
        std::condition_variable_any condVar;
        uint32_t imageIndex = 0;
        Frame* frame = nullptr;
        vk::Result result = vk::Result::eSuccess;
    } presentData{};

    std::jthread presentThread;

    void present(const std::stop_token& token);
};

}// namespace dragonfire::vulkan