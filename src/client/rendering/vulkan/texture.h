//
// Created by josh on 1/26/24.
//

#pragma once
#include "allocation.h"
#include <core/utility/string_hash.h>
#include <shared_mutex>

namespace dragonfire::vulkan {

class Texture final : public Image {};

class TextureRegistry {
public:
private:
    std::shared_mutex mutex;
    StringMap<Texture> textures;
};

}// namespace dragonfire::vulkan
