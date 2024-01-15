
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(PhysFS CONFIG REQUIRED)
find_package(unofficial-joltphysics CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(GameNetworkingSockets CONFIG REQUIRED)

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
)
target_link_libraries(dragonfire-core PUBLIC SDL2::SDL2 nlohmann_json::nlohmann_json fmt::fmt spdlog::spdlog unofficial::joltphysics::Jolt cxxopts::cxxopts
        $<IF:$<TARGET_EXISTS:PhysFS::PhysFS>,PhysFS::PhysFS,PhysFS::PhysFS-static> magic_enum::magic_enum GameNetworkingSockets::shared)