//
// Created by josh on 2/7/24.
//

#pragma once
#include "drawable.h"
#include "material.h"
#include <core/asset.h>
#include <glm/ext/matrix_transform.hpp>
#include <utility>

namespace dragonfire {
using Mesh = std::uintptr_t;

class Model : public Asset, public Drawable {
    friend class Loader;

public:
    Model() = default;

    Model(const std::string& name) { this->name = name; }

    ~Model() override = default;
    void writeDrawData(Drawables& drawables, const Transform& baseTransform) const override;

    class Loader : public AssetLoader {
    public:
        void setOptimize(const bool optimize) { optimizeMeshes = optimize; }

    protected:
        bool optimizeMeshes = true;
    };

    Model(const Model& other) : Asset(static_cast<Asset&>(*this)), primitives(other.primitives) {}

    Model(Model&& other) noexcept
        : Asset(static_cast<Asset&&>(*this)), primitives(std::move(other.primitives))
    {
    }

    Model& operator=(const Model& other)
    {
        if (this == &other)
            return *this;
        name = other.name;
        primitives = other.primitives;
        return *this;
    }

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

    struct Primitive {
        Mesh mesh = 0;
        std::shared_ptr<Material> material = nullptr;
        glm::vec4 bounds;
        glm::mat4 transform = glm::identity<glm::mat4>();
    };

    void addPrimitive(Primitive&& primitive) { primitives.push_back(primitive); }

    const Primitive& operator[](const size_t index) const { return primitives[index]; }

    [[nodiscard]] size_t primitiveCount() const { return primitives.size(); }

private:
    std::vector<Primitive> primitives;
};

}// namespace dragonfire
