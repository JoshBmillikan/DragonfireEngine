//
// Created by josh on 5/17/22.
//

#pragma once
#include "Service.h"
#include <entt/entt.hpp>

namespace dragonfire {

class EXPORTED World : public Service, public entt::registry {};

}   // namespace dragonfire