//
// Created by josh on 2/10/24.
//

#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <flecs.h>

namespace dragonfire {

class GameWorld {
    flecs::world world;
    JPH::PhysicsSystem physicsSystem;
};

}// namespace dragonfire
