//
// Created by josh on 5/19/22.
//

#pragma once
#include <Asset.h>
#include <fstream>

namespace dragonfire::rendering {
struct IMaterial : public Asset {};
IMaterial* loadMaterial(std::ifstream& stream);
}