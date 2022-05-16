//
// Created by Josh on 5/14/2022.
//

#pragma once
#include "Engine.h"
#include "Service.h"
#include <unordered_map>
#include <utility>
#include <variant>
#include <filesystem>

namespace dragonfire {
class Settings : public Service {
    typedef std::variant<
            int64_t,
            std::string,
            bool,
            double,
            std::vector<int64_t>,
            std::vector<std::string>,
            std::vector<double>,
            std::vector<bool>>
            ConfigTypes;
public:
    EXPORTED Settings();

    template<typename T>
    [[nodiscard]] T get(const std::string& str) {
        auto variant = configMap.at(str);
        if constexpr (std::is_same_v<T, bool>)
            return std::get<bool>(variant);
        else if constexpr (std::is_integral_v<T>)
            return static_cast<T>(std::get<int64_t>(variant));
        else if constexpr (std::is_floating_point_v<T>)
            return static_cast<T>(std::get<double>(variant));
        else if constexpr (std::is_same_v<std::string&, T>)
            return std::get<std::string>(variant);
        else return std::get<T>(variant);
    }

    template<typename T>
    [[nodiscard]] std::vector<T>& getVec(const std::string& str) {
        return std::get<std::vector<T>>(configMap.at(str));
    }

    template<typename T>
    T operator[](const std::string& str) {
        return get<T>(str);
    }

    inline void insert(const std::string& str, ConfigTypes&& val) {
        configMap[str] = std::move(val);
    }

private:
    void loadConfigDir(const std::filesystem::path& path);
    std::unordered_map<std::string, ConfigTypes> configMap;
};
}   // namespace dragonfire
