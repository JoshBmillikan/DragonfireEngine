//
// Created by Josh on 5/14/2022.
//

#include "Settings.h"
#include "FileLocator.h"
#include <toml++/toml.h>

using namespace dragonfire;
namespace fs = std::filesystem;

Settings::Settings() {
    auto& locator = Service::get<FileLocator>();
    fs::path defaultConfig(locator.baseDir);
    defaultConfig.append("Config");
    loadConfigDir(defaultConfig);
    loadConfigDir(locator.configDir);
}

typedef std::vector<std::tuple<
        std::string,
        std::variant<
                int64_t,
                std::string,
                bool,
                double,
                std::vector<int64_t>,
                std::vector<std::string>,
                std::vector<double>,
                std::vector<bool>>>>
        ConfigOptions;

template<typename T>
static std::vector<T> parseArray(const toml::array& array) {
    std::vector<T> vec;
    vec.reserve(array.size());
    for (auto& element : array) {
        vec.push_back(element.as<T>()->get());
    }

    return vec;
}

static void parseNodes(const toml::table& table, ConfigOptions& settings, const std::string& str, int depth = 0) {
    if (depth > 1000)
        throw std::runtime_error("Recursion depth exceeded");

    for (auto [key, val] : table) {
        std::string name(str);
        name.append(".");
        name.append(key.str());

        if (val.is_table())
            parseNodes(*val.as_table(), settings, name, depth + 1);

        else if (val.is_boolean())
            settings.emplace_back(name, val.as_boolean()->get());

        else if (val.is_floating_point())
            settings.emplace_back(name, val.as_floating_point()->get());

        else if (val.is_integer())
            settings.emplace_back(name, val.as_integer()->get());

        else if (val.is_string())
            settings.emplace_back(name, val.as_string()->get());

        else if (val.is_array()) {
            if (!val.is_homogeneous())
                throw std::runtime_error("Non-homogenous array are not supported");
            const auto& array = *val.as_array();

            if (array[0].is_boolean())
                settings.emplace_back(name, parseArray<bool>(array));

            else if (array[0].is_floating_point())
                settings.emplace_back(name, parseArray<double>(array));

            else if (array[0].is_integer())
                settings.emplace_back(name, parseArray<int64_t>(array));

            else if (array[0].is_string())
                settings.emplace_back(name, parseArray<std::string>(array));
        }
        else
            throw std::runtime_error("Unsupported config entry " + std::string(key.str()));
    }
}

void Settings::loadConfigDir(const fs::path& path) {
    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file() and entry.path().has_extension() and entry.path().extension() == ".toml") {
            try {
                toml::table table = toml::parse_file(entry.path().string());
                // Settings are first read into a vector, then copied to the map after parsing the rest of the file
                // This means an error in parsing will stop the map from being modified at all, partially valid files will be ignored
                ConfigOptions options;
                parseNodes(table, options, entry.path().stem().string());
                for (const auto& option : options) {
                    const auto [key, val] = option;
                    insert(key, val);
                }
                spdlog::info("Parsed config file {}", entry.path().string());
            }
            catch (const toml::parse_error& err) {
                spdlog::error("Failed to parse file {}, Error: {}", entry.path().string(), err.what());
            }
        }
    }
}
