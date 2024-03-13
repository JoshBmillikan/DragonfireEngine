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

    ~Model();

    class Loader {
    public:
        virtual ~Loader() = default;
        virtual Model load(const char* path) = 0;

        void setOptimize(const bool optimize) { optimizeMeshes = optimize; }

    protected:
        bool optimizeMeshes = true;
    };

    Model(const Model& other) = delete;

    Model(Model&& other) noexcept : name(std::move(other.name)), primitives(std::move(other.primitives)) {}

    Model& operator=(const Model& other) = delete;

    Model& operator=(Model&& other) noexcept
    {
        if (this == &other)
            return *this;
        name = std::move(other.name);
        primitives = std::move(other.primitives);
        return *this;
    }

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

    [[nodiscard]] size_t primitiveCount() const { return primitives.size(); }

private:
    std::string name;
    SmallVector<Primitive> primitives;
};

}// namespace dragonfire
