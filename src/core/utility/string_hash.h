//
// Created by josh on 8/11/23.
//

#pragma once
#include <ankerl/unordered_dense.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dragonfire {
/// Transparent string hasher, works with string_view as well as std::string
template<class Hasher = ankerl::unordered_dense::hash<std::string_view>>
struct StringHash {
    using is_transparent [[maybe_unused]] = void;// NOLINT(readability-identifier-naming)
    using is_avalanching [[maybe_unused]] = void;// NOLINT(readability-identifier-naming)

    std::size_t operator()(const char* str) const { return Hasher{}(str); }

    std::size_t operator()(const std::string_view str) const { return Hasher{}(str); }

    std::size_t operator()(std::string const& str) const { return Hasher{}(str); }
};

/// Hash map for mapping strings to T, uses transparent hasher and comparator, so can be accessed
/// with a string_view or const char* instead of a string&
template<typename T, typename Alloc = std::allocator<std::pair<const std::string, T>>>
using StringMap = std::unordered_map<std::string, T, StringHash<>, std::equal_to<>, Alloc>;
/// Flat map using a string hasher that can use be accesed with string_view or char*
template<typename T>
using StringFlatMap = ankerl::unordered_dense::map<std::string, T, StringHash<>, std::equal_to<>>;
/// Multi-map using a string hasher that can use be accesed with string_view or char*
template<typename T, typename Alloc = std::allocator<std::pair<const std::string, T>>>
using StringMultiMap = std::unordered_multimap<std::string, T, StringHash<>, std::equal_to<>, Alloc>;
}// namespace dragonfire