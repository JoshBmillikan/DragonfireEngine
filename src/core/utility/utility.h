//
// Created by josh on 10/10/23.
//

#pragma once
#include <cstdint>
#include <functional>

namespace dragonfire {

template<typename T, typename SEED = std::size_t>
constexpr void hashCombine(SEED& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

/// Returns the given size padded to be a multiple of the given alignment
constexpr std::size_t padToAlignment(const std::size_t s, const std::size_t alignment) noexcept
{
    return s + alignment - 1 & ~(alignment - 1);
}

constexpr std::size_t constStrlen(const char* str) noexcept
{
    if (str == nullptr)
        return 0;
    std::size_t i = 0;
    for (char c = str[0]; c != '\0'; c = *++str)
        i++;
    return i;
}

}// namespace raven