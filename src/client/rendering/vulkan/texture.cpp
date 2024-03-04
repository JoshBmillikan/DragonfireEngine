//
// Created by josh on 1/26/24.
//

#include "texture.h"

#include <mutex>

namespace dragonfire::vulkan {
TextureRegistry::TextureRegistry(GpuAllocator& allocator)
    : StagingBuffer(allocator, 0, true, "texture staging buffer")
{
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
        totalSize += image.len;
    SmallVector<Texture*> out;
    std::unique_lock lock(mutex);
    stbi_uc* ptr = static_cast<stbi_uc*>(getStagingPtr(totalSize));
    for (auto& [name, image] : data) {
        const auto& iter = textures.find(name);
        if (iter != textures.end()) {
            out.pushBack(&iter->second);
            continue;
        }

        memcpy(ptr, image.data.get(), image.len);
        ptr += image.len;
        // TODO gpu image copy
    }
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
