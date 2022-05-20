//
// Created by josh on 5/19/22.
//

#pragma once
#include "Service.h"

namespace dragonfire::rendering {
struct Asset {
    virtual ~Asset() noexcept = default;
};

class AssetManager : public Service {

};
}