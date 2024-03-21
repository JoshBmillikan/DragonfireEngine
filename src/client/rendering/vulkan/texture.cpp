//
// Created by josh on 1/26/24.
//

#include "texture.h"

#include "core/utility/temp_containers.h"

#include <mutex>
#include <ranges>

namespace dragonfire::vulkan {
Texture::Texture(const vk::Sampler sampler, const uint32_t id, Image&& image, const vk::Device device)
    : sampler(sampler), id(id), image(std::move(image)), device(device)
{
    constexpr vk::ImageSubresourceRange subresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    view = this->image.createView(device, subresourceRange);
}

Texture::~Texture()
{
    if (device) {
        device.destroy(view);
        device.destroy(sampler);
    }
}

Texture::Texture(Texture&& other) noexcept
    : sampler(other.sampler), id(other.id), image(std::move(other.image)), view(other.view),
      device(other.device)
{
    other.device = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this == &other)
        return *this;
    sampler = other.sampler;
    id = other.id;
    image = std::move(other.image);
    view = other.view;
    device = other.device;
    other.device = nullptr;
    return *this;
}

void Texture::writeDescriptor(const vk::DescriptorSet set, const uint32_t binding) const
{
    vk::DescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.imageView = view;
    vk::WriteDescriptorSet write{};
    write.descriptorCount = 1;
    write.dstArrayElement = id;
    write.dstSet = set;
    write.dstBinding = binding;
    write.pImageInfo = &imageInfo;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;

    device.updateDescriptorSets(write, {});
}

TextureRegistry::TextureRegistry(const Context& ctx, GpuAllocator& allocator)
    : StagingBuffer(allocator, 0, true, "texture staging buffer"), device(ctx.device), allocator(allocator),
      maxAnisotropy(ctx.deviceProperties->limits.maxSamplerAnisotropy), transferQueue(ctx.queues.graphics)
{
    fence = device.createFence(vk::FenceCreateInfo{});
    vk::CommandPoolCreateInfo createInfo{};
    createInfo.queueFamilyIndex = ctx.queues.graphicsFamily;
    pool = device.createCommandPool(createInfo);
    vk::CommandBufferAllocateInfo allocateInfo{};
    allocateInfo.level = vk::CommandBufferLevel::ePrimary;
    allocateInfo.commandPool = pool;
    allocateInfo.commandBufferCount = 1;
    vk::resultCheck(device.allocateCommandBuffers(&allocateInfo, &cmd), "Failed to allocate command buffer");
}

TextureRegistry::~TextureRegistry()
{
    if (device) {
        device.destroy(fence);
        device.destroy(pool);
        device = nullptr;
    }
}

Texture* TextureRegistry::getCreateTexture(const std::string_view name, ImageData&& image)
{
    std::array data = {std::tuple{name, std::move(image)}};
    return getCreateTextures(data)[0];
}

SmallVector<Texture*> TextureRegistry::getCreateTextures(
    const std::span<std::tuple<std::string_view, ImageData>> data
)
{
    vk::DeviceSize totalSize = 0;
    for (auto& [name, image] : data)
        totalSize += image.x * image.y * image.channels;
    SmallVector<Texture*> out;
    std::unique_lock lock(mutex);
    stbi_uc* ptr = static_cast<stbi_uc*>(getStagingPtr(totalSize));
    device.resetCommandPool(pool);
    constexpr vk::CommandBufferBeginInfo beginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    cmd.begin(beginInfo);
    vk::DeviceSize offset = 0;
    for (auto& [name, image] : data) {
        const auto& iter = textures.find(name);
        if (iter != textures.end()) {
            out.pushBack(&iter->second);
            continue;
        }

        const size_t imageSize = image.x * image.y * image.channels;
        const stbi_uc* imagePtr = image.data.get();
        memcpy(ptr + offset, imagePtr, imageSize);

        vk::ImageCreateInfo createInfo{};
        createInfo.extent.width = image.x;
        createInfo.extent.height = image.y;
        createInfo.extent.depth = 1;
        createInfo.samples = vk::SampleCountFlagBits::e1;
        createInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        createInfo.format = vk::Format::eR8G8B8A8Srgb;
        createInfo.imageType = vk::ImageType::e2D;
        createInfo.mipLevels = 1;
        createInfo.arrayLayers = 1;
        createInfo.tiling = vk::ImageTiling::eOptimal;
        createInfo.initialLayout = vk::ImageLayout::eUndefined;
        createInfo.sharingMode = vk::SharingMode::eExclusive;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        allocInfo.priority = 1.0f;

        std::string s(name);
        Image i = allocator.allocate(createInfo, allocInfo, s.c_str());
        vk::ImageMemoryBarrier barrier{};
        barrier.image = i;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstQueueFamilyIndex = barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            {},
            {},
            barrier
        );

        vk::BufferImageCopy copy{};
        copy.bufferOffset = offset;
        copy.bufferRowLength = 0;
        copy.bufferImageHeight = 0;
        copy.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        copy.imageOffset = vk::Offset3D{0, 0, 0};
        copy.imageExtent.width = image.x;
        copy.imageExtent.height = image.y;
        copy.imageExtent.depth = 1;

        cmd.copyBufferToImage(getStagingBuffer(), i, vk::ImageLayout::eTransferDstOptimal, copy);

        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            {},
            {},
            barrier
        );

        offset += imageSize;

        vk::SamplerCreateInfo samplerCreateInfo{};// TODO create from texture info
        samplerCreateInfo.magFilter = samplerCreateInfo.minFilter = vk::Filter::eLinear;
        samplerCreateInfo.addressModeU = samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeW
            = vk::SamplerAddressMode::eRepeat;
        samplerCreateInfo.anisotropyEnable = true;
        samplerCreateInfo.maxAnisotropy = maxAnisotropy;
        samplerCreateInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerCreateInfo.unnormalizedCoordinates = false;
        samplerCreateInfo.compareEnable = false;
        samplerCreateInfo.compareOp = vk::CompareOp::eAlways;
        samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerCreateInfo.mipLodBias = samplerCreateInfo.minLod = samplerCreateInfo.maxLod = 0.0f;
        vk::Sampler sampler = device.createSampler(samplerCreateInfo);
        auto& t = textures[s] = Texture(sampler, ++textureCount, std::move(i), device);
        out.pushBack(&t);
    }
    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    transferQueue.submit(submitInfo, fence);
    if (device.waitForFences(fence, true, UINT64_MAX) != vk::Result::eSuccess)
        throw std::runtime_error("Failed to wait for fence");
    device.resetFences(fence);
    return out;
}

Texture* TextureRegistry::getTexture(const std::string_view name)
{
    std::shared_lock lock(mutex);
    const auto iter = textures.find(name);
    if (iter == textures.end())
        return nullptr;
    return &iter->second;
}

}// namespace dragonfire::vulkan
