//
// Created by josh on 11/15/23.
//

#pragma once
#include "frame_allocator.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace dragonfire {

using TempString = std::basic_string<char, std::char_traits<char>, FrameAllocator<char>>;

template<typename T>
using TempVec = std::vector<T, FrameAllocator<T>>;

template<typename K, typename V>
using TempMap = std::map<K, V, std::less<K>, FrameAllocator<std::pair<K, V>>>;

template<typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
using TempHashMap = std::unordered_map<K, V, Hash, Eq, FrameAllocator<std::pair<const K, V>>>;

}// namespace raven