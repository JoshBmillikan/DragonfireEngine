//
// Created by josh on 1/26/24.
//

#pragma once
#include "allocation.h"
#include "core/utility/small_vector.h"
#include <client/rendering/material.h>
#include <core/utility/string_hash.h>
#include <memory>
#include <shared_mutex>
#include <stb_image.h>

namespace dragonfire::vulkan {

class Texture final : public Image {
    vk::Sampler sampler;
    uint32_t id = 0;
};

struct ImageData {
    using DataType = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    DataType data = {nullptr, &stbi_image_free};
    size_t len = 0;
    int x = 0, y = 0, channels = 0;
    ImageData() = default;

    void setData(stbi_uc* ptr) { data = DataType(ptr, &stbi_image_free); }
};

class TextureRegistry {
public:
    Texture* getCreateTexture(std::string_view name, ImageData&& image);
    SmallVector<Texture*> getCreateTextures(std::span<std::string_view> names, std::span<ImageData> images);

private:
    std::shared_mutex mutex;
    StringMap<Texture> textures;
    uint32_t textureCount = 0;
    Buffer stagingBuffer;
};

}// namespace dragonfire::vulkan
