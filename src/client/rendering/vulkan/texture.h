//
// Created by josh on 1/26/24.
//

#pragma once
#include "allocation.h"
#include <core/utility/string_hash.h>
#include <shared_mutex>

namespace dragonfire::vulkan {
struct alignas(16)  TextureIds {
    uint32_t albedo = 0;
    uint32_t normal = 0;
    uint32_t ambient = 0;
    uint32_t diffuse = 0;
    uint32_t specular = 0;
};

class Texture final : public Image {};

class TextureRegistry {
public:
private:
    std::shared_mutex mutex;
    StringMap<Texture> textures;
};

}// namespace dragonfire::vulkan
