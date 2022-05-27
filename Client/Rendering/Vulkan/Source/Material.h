//
// Created by josh on 5/19/22.
//

#pragma once
#include <IMaterial.h>
#include <filesystem>
#include <SQLiteCpp/Database.h>

namespace dragonfire::rendering {
class Material : public IMaterial {

public:
    inline vk::PipelineLayout getLayout() {
        return nullptr;
    }

    void bind(vk::CommandBuffer) const noexcept;
};

class MaterialFactory {
    vk::PipelineCache cache;
    vk::Device device;
    SQLite::Database db;
    std::unordered_map<std::string, std::weak_ptr<Material>> materials;
public:
    explicit MaterialFactory(vk::Device device);
    ~MaterialFactory() noexcept;

    std::shared_ptr<Material> createMaterial(const std::string& materialName);
};
}   // namespace dragonfire::rendering