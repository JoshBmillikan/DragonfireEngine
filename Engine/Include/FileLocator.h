//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "Service.h"
#include <filesystem>

namespace dragonfire {
class FileLocator : public Service {
public:
    EXPORTED FileLocator();
    const std::filesystem::path baseDir, assetDir, dataDir, configDir;
};
}   // namespace dragonfire
