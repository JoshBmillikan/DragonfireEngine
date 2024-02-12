//
// Created by josh on 2/7/24.
//

#pragma once
#include "core/utility/small_vector.h"
#include "material.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <utility>

namespace dragonfire {
using Mesh = std::uintptr_t;

class Model {
    friend class Loader;

public:
    Model() = default;

    Model(std::string name) : name(std::move(name)) {}

    class Loader {
    public:
        virtual ~Loader() = default;
        virtual Model load(const char* path) = 0;

        void setOptimize(const bool optimize) { optimizeMeshes = optimize; }

    protected:
        bool optimizeMeshes = true;
    };

    void addPrimitive(
        Mesh mesh,
        Material* material,
        glm::vec4 bounds,
        const glm::mat4& transform = glm::identity<glm::mat4>()
    );

    [[nodiscard]] const std::string& getName() const { return name; }

    struct Primitive {
        Mesh mesh = 0;
        Material* material = nullptr;
        glm::vec4 bounds;
        glm::mat4 transform = glm::identity<glm::mat4>();
    };

    void addPrimitive(Primitive&& primitive) { primitives.pushBack(primitive); }

    const Primitive& operator[](const size_t index) const { return primitives[index]; }

private:
    std::string name;
    SmallVector<Primitive> primitives;
};

}// namespace dragonfire
