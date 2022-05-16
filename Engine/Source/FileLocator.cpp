//
// Created by Josh on 5/14/2022.
//

#include "FileLocator.h"
#include <SDL_filesystem.h>

namespace fs = std::filesystem;

static fs::path getDataDir() {
    char* path = SDL_GetPrefPath("Dragonfire", APP_NAME);
    auto p = fs::path(path);
    SDL_free(path);
    return canonical(p);
}

EXPORTED dragonfire::FileLocator::FileLocator()
    : baseDir(fs::current_path().parent_path()), assetDir(fs::path(baseDir).append("Assets")), dataDir(getDataDir()), configDir(fs::path(dataDir).append("Config")) {
    create_directories(dataDir);
    create_directories(configDir);
    create_directories(assetDir);
    spdlog::info("Asset dir: {}", assetDir.string());
    spdlog::info("Data dir: {}", dataDir.string());
    spdlog::info("Config dir: {}", configDir.string());
}
