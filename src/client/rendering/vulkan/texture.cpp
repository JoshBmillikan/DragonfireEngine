//
// Created by josh on 1/26/24.
//

#include "texture.h"

namespace dragonfire::vulkan {
Texture* TextureRegistry::getCreateTexture(const std::string_view name, ImageData&& image)
{
    std::array names = {name};
    std::array images = {std::move(image)};
    return getCreateTextures(names, images)[0];
}

SmallVector<Texture*> TextureRegistry::getCreateTextures(
    std::span<std::string_view> names,
    std::span<ImageData> images
)
{
}
}// namespace dragonfire::vulkan
