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
#include "texture.h"
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
        vk::CommandBuffer cmd;
        vk::CommandPool pool;
        vk::DescriptorSet globalDescriptorSet, computeSet, frameSet;
        Buffer drawData, culledMatrices, commandBuffer, countBuffer, textureIndexBuffer;
        vk::Fence fence;
        uint32_t textureBinding = 0;
        Frame() = default;
        Frame(const Context& ctx, const GpuAllocator& allocator, size_t maxDrawCount);
    };

    static constexpr int FRAMES_IN_FLIGHT = 2;
    const size_t maxDrawCount;

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

    struct alignas(16) DrawData {
        glm::mat4 transform{};
        uint32_t vertexOffset = 0, vertexCount = 0, indexOffset = 0, indexCount = 0;
        TextureIds textureIndices;
        glm::vec4 boundingSphere;
    };

    struct PipelineDrawInfo {
        uint32_t index = 0, drawCount = 0;
        vk::PipelineLayout layout;
    };

    void present(const std::stop_token& token);
};

}// namespace dragonfire::vulkan