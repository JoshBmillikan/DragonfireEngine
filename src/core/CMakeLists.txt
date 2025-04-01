
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(GameNetworkingSockets CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LuaJIT REQUIRED IMPORTED_TARGET luajit)
find_package(simdjson CONFIG REQUIRED)

add_library(dragonfire-core STATIC
        engine.cpp
        engine.h
        file.cpp
        file.h
        utility/formatted_error.h
        utility/frame_allocator.cpp
        utility/frame_allocator.h
        utility/string_hash.h
        utility/temp_containers.h
        utility/utility.h
        crash.cpp
        crash.h
        config.cpp
        config.h
        utility/rng.cpp
        utility/rng.h
        utility/math_utils.h
        utility/small_vector.h
        world/transform.h
        world/game_world.cpp
        world/game_world.h
        asset.cpp
        asset.h
        event.cpp
        event.h
        voxel/terrain.cpp
        voxel/terrain.h
        voxel/voxel.cpp
        voxel/voxel.h
        voxel/chunk.cpp
        voxel/chunk.h
)
target_link_libraries(dragonfire-core PUBLIC SDL2::SDL2 nlohmann_json::nlohmann_json fmt::fmt
        glm::glm spdlog::spdlog Jolt cxxopts::cxxopts simdjson::simdjson
        physfs-static
        magic_enum::magic_enum GameNetworkingSockets::static unordered_dense::unordered_dense
        $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static> FastNoise sol2::sol2 PkgConfig::LuaJIT)
target_compile_definitions(dragonfire-core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_ENABLE_EXPERIMENTAL)

add_executable(core-tests utility/frame_allocator.test.cpp
        utility/small_vector.test.cpp
        voxel/voxel.test.cpp)
target_link_libraries(core-tests PRIVATE dragonfire-core Catch2::Catch2WithMain)
catch_discover_tests(core-tests)