//
// Created by josh on 1/14/24.
//

#pragma once
#include "utility/string_hash.h"
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <variant>

namespace dragonfire {

class Config {
public:
    using Variable = std::variant<int64_t, double, bool, std::string>;

    static Config& get() noexcept { return INSTANCE; }

    std::optional<int64_t> getInt(std::string_view id) const;
    std::optional<double> getFloat(std::string_view id) const;
    std::optional<bool> getBool(std::string_view id) const;
    std::optional<std::pair<const std::string&, std::shared_lock<std::shared_mutex>>> getString(
        std::string_view id
    ) const;
    std::optional<std::pair<const Variable&, std::shared_lock<std::shared_mutex>>> getVariable(
        std::string_view id
    ) const;

    template<typename T>
    void setVar(std::string&& id, T data)
    {
        std::unique_lock lock(mutex);
        vars[id] = std::forward<T>(data);
    }

    void loadJson(std::string& txt);
    void loadJsonFile(const char* path);
    std::string toJson() const;
    void saveJson(const char* path) const;

private:
    static Config INSTANCE;
    mutable std::shared_mutex mutex;
    StringMap<Variable> vars;
};

}// namespace raven