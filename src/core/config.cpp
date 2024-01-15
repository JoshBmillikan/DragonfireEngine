//
// Created by josh on 1/14/24.
//

#include "config.h"
#include "file.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace dragonfire {

template<typename T>
static std::optional<T> extractVar(auto v)
{
    if (v.has_value()) {
        auto lock = std::move(v.value().second);
        if (v.value().first.data.has_value())
            return std::get<T>(v.value().first.data.value());
    }
    return std::nullopt;
}

std::optional<int64_t> Config::getInt(const std::string_view id) const
{
    auto v = getVariable(id);
    return extractVar<int64_t>(std::move(v));
}

std::optional<double> Config::getFloat(const std::string_view id) const
{
    auto v = getVariable(id);
    return extractVar<double>(std::move(v));
}

std::optional<bool> Config::getBool(const std::string_view id) const
{
    auto v = getVariable(id);
    return extractVar<bool>(std::move(v));
}

std::optional<std::pair<const std::string&, std::shared_lock<std::shared_mutex>>> Config::getString(
    const std::string_view id
) const
{
    auto v = getVariable(id);
    if (v.has_value()) {
        auto [var, lock] = std::move(v.value());
        if (var.data.has_value())
            return std::pair{std::get<std::string>(var.data.value()), std::move(lock)};
    }
    return std::nullopt;
}

std::optional<std::pair<const Config::Variable&, std::shared_lock<std::shared_mutex>>> Config::getVariable(
    const std::string_view id
) const
{
    std::shared_lock lock(mutex);
    const auto iter = vars.find(id);
    if (iter == vars.end())
        return std::nullopt;
    return std::pair{iter->second, std::move(lock)};
}

void Config::loadJson(std::string& txt)
{
    auto json = nlohmann::json::parse(txt);
    std::unique_lock lock(mutex);
    for (auto& [key, value] : json.items()) {
        switch (value.type()) {
            case nlohmann::detail::value_t::null: vars[key]; break;
            case nlohmann::detail::value_t::string: vars.emplace(key, value.get<std::string>()); break;
            case nlohmann::detail::value_t::boolean: vars.emplace(key, value.get<bool>()); break;
            case nlohmann::detail::value_t::number_integer:
            case nlohmann::detail::value_t::number_unsigned: vars.emplace(key, value.get<int64_t>()); break;
            case nlohmann::detail::value_t::number_float: vars.emplace(key, value.get<double>()); break;
            default: SPDLOG_ERROR("Invalid config value for json key {}", key);
        }
    }
}

void Config::loadJsonFile(const char* path)
{
    File file(path);
    auto txt = file.readString();
    file.close();
    loadJson(txt);
}

std::string Config::toJson() const

{
    std::shared_lock lock(mutex);
    nlohmann::json json;
    for (auto& [key, value] : vars) {
        if (value.data.has_value())
            std::visit([&](const auto& arg) { json[key] = arg; }, value.data.value());
        else
            json[key];
    }

    return to_string(json);
}

void Config::saveJson(const char* path) const
{
    const auto txt = toJson();
    File file(path, File::Mode::WRITE);
    file.write(txt);
}
}// namespace raven