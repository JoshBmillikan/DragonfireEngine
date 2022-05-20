//
// Created by josh on 5/19/22.
//

#pragma once
#include <IMaterial.h>

namespace dragonfire::rendering {
class Material : public IMaterial {
public:
    explicit Material(std::ifstream& stream);
};
}