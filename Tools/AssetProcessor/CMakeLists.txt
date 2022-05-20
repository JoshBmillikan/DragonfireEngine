
add_executable(AssetProcessor Source/Main.cpp Source/Material.cpp Source/Material.h)
target_link_libraries(AssetProcessor PUBLIC libshaderc nlohmann_json::nlohmann_json)