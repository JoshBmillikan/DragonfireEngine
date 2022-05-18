
add_library(Engine SHARED Include/Engine.h Source/Engine.cpp Include/Service.h Source/Game.cpp Include/Game.h Source/FileLocator.cpp Include/FileLocator.h Source/Settings.cpp Include/Settings.h Source/World.cpp Include/World.h)
target_include_directories(Engine PUBLIC Include)
target_compile_definitions(Engine PRIVATE _DLL)
target_link_libraries(Engine PUBLIC spdlog::spdlog glm::glm SDL2::SDL2 tomlplusplus::tomlplusplus re2::re2 EnTT::EnTT)
target_precompile_headers(Engine PUBLIC <vector> <algorithm> <memory> <stdexcept> <spdlog/spdlog.h> <tuple> <glm/glm.hpp> <cstdint> <string>)