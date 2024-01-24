//
// Created by josh on 1/23/24.
//
#include "small_vector.h"
#include <catch.hpp>

using namespace dragonfire;

TEST_CASE("Small Vector basic usage")
{
    SmallVector<int> vec;
    vec.pushBack(1);
    vec.pushBack(5);
    vec.pushBack(1);
    REQUIRE(!vec.isSpilled());

    SECTION("Spilled")
    {
        for (int i = 0; i < 1000; i++)
            vec.pushBack(i);
        CHECK(vec.isSpilled());
    }

    SECTION("Index")
    {
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 5);
        CHECK(vec[2] == 1);
    }

    SECTION("Iterate")
    {
        int c = 0;
        for (int i : vec) {
            c += i;
        }
        CHECK(c == 7);
    }
}
