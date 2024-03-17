//
// Created by josh on 1/26/24.
//

#pragma once
#include "allocation.h"
#include "context.h"
#include "core/utility/small_vector.h"
#include "staging_buffer.h"
#include <client/rendering/material.h>
#include <core/utility/string_hash.h>
#include <memory>
#include <shared_mutex>
#include <stb_image.h>

namespace dragonfire::vulkan {

class Texture {
    vk::Sampler sampler;
    uint32_t id = 0;
    Image image;
    vk::ImageView view;
    vk::Device device;

public:
    Texture(vk::Sampler sampler, uint32_t id, Image&& image, vk::Device device);
    ~Texture();
    Texture() = default;
    Texture(const Texture& other) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(const Texture& other) = delete;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] vk::Sampler getSampler() const { return sampler; }

    [[nodiscard]] uint32_t getId() const { return id; }

    void writeDescriptor(vk::DescriptorSet set, uint32_t binding) const;
};

struct ImageData {
    using DataType = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    DataType data = {nullptr, &stbi_image_free};
    size_t len = 0;
    int x = 0, y = 0, channels = 0;
    ImageData() = default;

    void setData(stbi_uc* ptr) { data = DataType(ptr, &stbi_image_free); }
};

class TextureRegistry final : public StagingBuffer {
public:
    TextureRegistry(const Context& ctx, GpuAllocator& allocator);
    ~TextureRegistry() override;

    TextureRegistry(const TextureRegistry& other) = delete;
    TextureRegistry(TextureRegistry&& other) noexcept = delete;
    TextureRegistry& operator=(const TextureRegistry& other) = delete;
    TextureRegistry& operator=(TextureRegistry&& other) noexcept = delete;

    Texture* getCreateTexture(std::string_view name, ImageData&& image);
    SmallVector<Texture*> getCreateTextures(std::span<std::tuple<std::string_view, ImageData>> data);

    Texture* getTexture(std::string_view name);

private:
    std::shared_mutex mutex;
    StringMap<Texture> textures;
    uint32_t textureCount = 0;
    vk::CommandBuffer cmd;
    vk::CommandPool pool;
    vk::Device device;
    GpuAllocator& allocator;
    float maxAnisotropy;
    vk::Queue transferQueue;
    vk::Fence fence;
};

}// namespace dragonfire::vulkan
