//
// Created by Josh on 5/14/2022.
//

#include "Settings.h"
#include "FileLocator.h"
#include <toml++/toml.h>

using namespace dragonfire;
namespace fs = std::filesystem;

EXPORTED Settings::Settings() {
    auto& locator = Service::get<FileLocator>();
    fs::path defaultConfig(locator.baseDir);
    defaultConfig.append("Config");
    loadConfigDir(defaultConfig);
    loadConfigDir(locator.configDir);
}

void Settings::loadConfigDir(const std::filesystem::path& path) {
    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file() and entry.path().has_extension() and entry.path().extension() == ".toml") {
            try {
                toml::table table = toml::parse_file(path.string());
                spdlog::info("Pared config file {}", entry.path().string());
            }
            catch (const toml::parse_error& err) {
                spdlog::error("Failed to parser file {}, Error: {}", entry.path().string(), err.what());
            }
        }
    }
}
