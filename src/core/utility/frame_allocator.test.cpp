//
// Created by josh on 10/17/23.
//
#include <catch.hpp>
#include <core/utility/temp_containers.h>
#include <core/utility/frame_allocator.h>

using namespace dragonfire;

TEST_CASE("Frame Allocator Test")
{
    frameAllocator::nextFrame();

    SECTION("Alloc")
    {
        auto ptr = static_cast<char*>(frameAllocator::alloc(1024));
        CHECK(ptr != nullptr);
        strcpy(ptr, "hello");
        CHECK(strcmp(ptr, "hello") == 0);
        CHECK(frameAllocator::alloc(0) == nullptr);
    }

    SECTION("Free")
    {
        void* ptr = frameAllocator::alloc(1024);
        CHECK(ptr != nullptr);
        CHECK(frameAllocator::freeLast(ptr, 1024));
    }

    SECTION("Over alloc")
    {
        REQUIRE(frameAllocator::alloc(UINT64_MAX) == nullptr);
    }
}

TEST_CASE("Temporary Containers Test")
{
    REQUIRE_NOTHROW([]() {
        TempVec<std::uint32_t> data;
        TempString str = "Hello";

        for (std::uint32_t i = 0; i < 1000; i++)
            data.push_back(i);
        return !data.empty();
    });
}