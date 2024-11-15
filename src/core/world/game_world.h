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
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

public:
    explicit GameWorld(uint32_t maxBodies = 10240);
    flecs::world& getECSWorld() { return world; }
};

}// namespace dragonfire
