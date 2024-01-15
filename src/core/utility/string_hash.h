//
// Created by josh on 8/11/23.
//

#pragma once
#include <string>
#include <string_view>
#include <unordered_map>

namespace dragonfire {
/// Transparent string hasher, works with string_view as well as std::string
template<class Hasher = std::hash<std::string_view>>
struct StringHash {
    using is_transparent [[maybe_unused]] = void;// NOLINT(readability-identifier-naming)

    std::size_t operator()(const char* str) const { return Hasher{}(str); }

    std::size_t operator()(const std::string_view str) const { return Hasher{}(str); }

    std::size_t operator()(std::string const& str) const { return Hasher{}(str); }
};

/// Hash map for mapping strings to T, uses transparent hasher and comparator, so can be accessed
/// with a string_view or const char* instead of a string&
template<typename T>
using StringMap = std::unordered_map<std::string, T, StringHash<>, std::equal_to<>>;
}// namespace raven