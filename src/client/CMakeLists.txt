find_package(fastgltf CONFIG REQUIRED)
add_library(dragonfire-client STATIC
        app.cpp
        app.h)
target_link_libraries(dragonfire-client PUBLIC dragonfire-core fastgltf::fastgltf)

add_executable(${APP_NAME} main.cpp)
target_link_libraries(${APP_NAME} PRIVATE dragonfire-client SDL2::SDL2main)