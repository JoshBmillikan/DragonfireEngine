//
// Created by josh on 1/15/24.
// Random number generators based on xoshiro256
// https://prng.di.unimi.it/

#include "rng.h"
#include <SDL_timer.h>
#include <bit>

namespace dragonfire {
RNG::RNG(uint64_t seed)
{
    for (auto& state : s) {
        uint64_t z = seed += 0x9e3779b97f4a7c15;
        z = (z ^ z >> 30) * 0xbf58476d1ce4e5b9;
        z = (z ^ z >> 27) * 0x94d049bb133111eb;
        state = z ^ z >> 31;
    }
}

RNG::RNG() : RNG(SDL_GetTicks64()) {}

// xoshiro256++
uint64_t RNG::next()
{
    const uint64_t result = std::rotl(s[0] + s[3], 23) + s[0];
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = std::rotl(s[3], 45);

    return result;
}

// xoshiro256+
double RNG::nextDouble()
{
    const uint64_t result = s[0] + s[3];
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = std::rotl(s[3], 45);

    return double(result >> 11) * 0x1.0p-53;
}
}// namespace dragonfire