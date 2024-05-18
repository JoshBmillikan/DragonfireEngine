//
// Created by josh on 4/29/24.
//

#include "asset.h"
#include "file.h"
#include <algorithm>
#include <cstring>
#include <physfs.h>
#include <ranges>
#include <spdlog/spdlog.h>

namespace dragonfire {

void AssetManager::loadDirectory(const char* dir, AssetLoader* loader)
{
    assert(dir && loader);
    const auto exts = loader->acceptedFileExtensions();
    char** ls = PHYSFS_enumerateFiles(dir);
    if (ls == nullptr)
        throw PhysFsError(dir);

    std::unique_lock lock(mutex);
    std::string path;
    for (char** ptr = ls; *ptr; ptr++) {
        const char* end = strrchr(*ptr, '.');
        const bool hasExtensions = std::ranges::any_of(exts, [end](const char* ext) {
            if (strcmp(end, ext) == 0)
                return true;
            return false;
        });
        if (!hasExtensions)
            continue;
        try {
            path.clear();
            path = dir;
            if (!path.ends_with("/"))
                path += "/";
            path += *ptr;
            Asset* asset = loader->load(path.c_str());
            auto& entry = assets[asset->getName()];
            entry.asset = std::unique_ptr<Asset>(asset);
            entry.filePath = path;
            spdlog::info("Loaded asset \"{}\"", asset->getName());
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to load asset file \"{}\", error: {}", path, e.what());
        }
    }

    PHYSFS_freeList(ls);
}

void AssetManager::destroyAsset(const std::string_view id)
{
    std::unique_lock lock(mutex);
    const auto& found = assets.find(id);
    if (found == assets.end())
        throw std::out_of_range("Asset not loaded");

    uint64_t prev = found->second.count.exchange(0);
    if (prev > 0)
        SPDLOG_WARN("Asset \"{}\" was deleted but still had {} references alive", found->first, prev);
    found->second.asset.reset();
    found->second.filePath.clear();
}

void AssetManager::clear()
{
    std::unique_lock lock(mutex);
    for (auto& [name, entry] : assets) {
        uint64_t prev = entry.count.exchange(0);
        if (prev > 0)
            SPDLOG_WARN("Asset \"{}\" was deleted but still had {} references alive", name, prev);
        entry.asset.reset();
        entry.filePath.clear();
    }
    assets.clear();
}

AssetManager::AssetManager(AssetManager&& other) noexcept
{
    if (this != &other) {
        std::scoped_lock lock(mutex, other.mutex);
        assets = std::move(other.assets);
    }
}

AssetManager& AssetManager::operator=(AssetManager&& other) noexcept
{
    if (this == &other)
        return *this;
    std::scoped_lock lock(mutex, other.mutex);
    assets = std::move(other.assets);
    return *this;
}
}// namespace dragonfire