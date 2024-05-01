//
// Created by josh on 4/29/24.
//

#pragma once
#include "utility/string_hash.h"
#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>

namespace dragonfire {

class Asset {
public:
    virtual ~Asset() = default;

    [[nodiscard]] const std::string& getName() const { return name; }

protected:
    std::string name;
};

struct AssetLoader {
    virtual ~AssetLoader() = default;
    virtual Asset* load(const char* path) = 0;
    virtual std::span<const char*> acceptedFileExtensions() = 0;
};

struct AssetEntry {
    std::unique_ptr<Asset> asset = nullptr;
    mutable std::atomic_uint64_t count = 0;
    std::string filePath;
};

template<typename T>
    requires std::is_base_of_v<Asset, T>
class AssetRef {
    const AssetEntry* entry = nullptr;

public:
    inline static AssetRef NULL_REF;

    T& operator*() { return *static_cast<T*>(entry->asset.get()); }

    T& operator*() const { return *static_cast<T*>(entry->asset.get()); }

    T* operator->() { return static_cast<T*>(entry->asset.get()); }

    operator T*() { return static_cast<T*>(entry->asset.get()); }

    operator const T*() const { return static_cast<T*>(entry->asset.get()); }

    operator bool() const { return entry; }

    AssetRef() = default;

    AssetRef(AssetEntry* entry) noexcept : entry(entry)
    {
        assert(dynamic_cast<T*>(entry->asset.get()));
        entry->count++;
    }

    AssetRef(const AssetEntry* entry) noexcept : entry(entry)
    {
        assert(dynamic_cast<T*>(entry->asset.get()));
        entry->count++;
    }

    ~AssetRef() noexcept
    {
        if (entry)
            entry->count--;
    }

    AssetRef(const AssetRef& other)
    {
        other.entry->count++;
        entry = other.entry;
    }

    AssetRef(AssetRef&& other) noexcept : entry(other.entry) { other.entry = nullptr; }

    AssetRef& operator=(const AssetRef& other)
    {
        if (this == &other)
            return *this;
        other.entry->count++;
        entry = other.entry;
        return *this;
    }

    AssetRef& operator=(AssetRef&& other) noexcept
    {
        if (this == &other)
            return *this;
        entry = other.entry;
        other.entry = nullptr;
        return *this;
    }

    auto operator<=>(const AssetRef& other) const { return entry <=> (other.entry); }
};

class AssetRegistry {
public:
    AssetRegistry() = default;

    template<typename T>
        requires std::is_base_of_v<Asset, T>
    AssetRef<T> get(const std::string_view id) const
    {
        std::shared_lock lock(mutex);
        const auto& found = assets.find(id);
        if (found == assets.end())
            return AssetRef<T>::NULL_REF;
        const AssetEntry* entry = &found->second;
        return AssetRef<T>(entry);
    }

    void loadDirectory(const char* dir, AssetLoader* loader);
    void destroyAsset(std::string_view id);
    void clear();

    ~AssetRegistry() { clear(); }

    AssetRegistry(const AssetRegistry& other) = delete;
    AssetRegistry(AssetRegistry&& other) noexcept;
    AssetRegistry& operator=(const AssetRegistry& other) = delete;
    AssetRegistry& operator=(AssetRegistry&& other) noexcept;

private:
    mutable std::shared_mutex mutex;
    StringMap<AssetEntry> assets;
};

}// namespace dragonfire
