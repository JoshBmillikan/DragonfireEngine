//
// Created by josh on 5/19/22.
//

#pragma once
#include <IMaterial.h>
#include <Service.h>
#include <filesystem>
#include <SQLiteCpp/Database.h>

namespace dragonfire::rendering {
class Material : public IMaterial {
public:
};

class MaterialFactory : Service {
    vk::PipelineCache cache;
    vk::Device device;
    SQLite::Database db;
public:
    explicit MaterialFactory(vk::Device device);
    ~MaterialFactory() override;

    Material* createMaterial(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::in | std::ios::binary);
        return createMaterial(stream, path.stem());
    }

    Material* createMaterial(std::ifstream& stream, const std::string& materialName);
};
}   // namespace dragonfire::rendering