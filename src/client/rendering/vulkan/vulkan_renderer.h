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

    static constexpr int FRAMES_IN_FLIGHT = 2;
    const uint32_t maxDrawCount;

protected:
    void beginFrame(const Camera& camera) override;
    void drawModels(const Camera& camera, const Drawables& models) override;
    void endFrame() override;

private:
    struct Frame {
        vk::Semaphore renderingSemaphore, presentSemaphore;
        vk::CommandBuffer cmd;
        vk::CommandPool pool;
        vk::DescriptorSet globalDescriptorSet, computeSet, frameSet;
        Buffer drawData, culledMatrices, commandBuffer, countBuffer, textureIndexBuffer;
        vk::Fence fence;
        uint32_t textureBinding = 0;
        Frame() = default;
        Frame(const Context& ctx, const GpuAllocator& allocator, uint32_t maxDrawCount);
    };

    Context context;
    GpuAllocator allocator;
    Swapchain swapchain;
    std::unique_ptr<MeshRegistry> meshRegistry;
    DescriptorLayoutManager descriptorLayoutManager;
    std::unique_ptr<PipelineFactory> pipelineFactory;
    Image depthBuffer, msaaImage;
    vk::ImageView depthView, msaaView;
    std::array<Frame, FRAMES_IN_FLIGHT> frames;
    Buffer globalUBO;
    vk::DeviceSize uboOffset = 0;
    Pipeline cullPipeline;
    vk::PipelineLayout cullComputeLayout;
    std::vector<Pipeline> pipelines;

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

    ankerl::unordered_dense::map<vk::Pipeline, PipelineDrawInfo> pipelineMap;

    void computePrePass(uint32_t drawCount, bool cull);
    void waitForLastFrame();
    void writeGlobalUBO(const Camera& camera) const;
    void present(const std::stop_token& token);

    const Frame& getCurrentFrame() const { return frames[getFrameCount() % FRAMES_IN_FLIGHT]; }

    Frame& getCurrentFrame() { return frames[getFrameCount() % FRAMES_IN_FLIGHT]; }
};

struct UBOData {
    alignas(16) glm::mat4 perspective;
    alignas(16) glm::mat4 orthographic;
    alignas(16) glm::mat4 view;
    alignas(16) glm::vec3 sunDirection;
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::vec2 resolution;
    alignas(16) glm::vec4 frustum;
    float P00, P11, zNear, zFar;
};

}// namespace dragonfire::vulkan