//
// Created by josh on 1/15/24.
//

#pragma once
#include <cstdint>

namespace dragonfire {

class RNG {
    uint64_t s[4]{};

public:
    explicit RNG(uint64_t seed);
    RNG();
    uint64_t next();
    double nextDouble();
};

}// namespace dragonfire