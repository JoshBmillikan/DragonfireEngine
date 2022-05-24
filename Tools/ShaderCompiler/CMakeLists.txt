
add_executable(ShaderCompiler Main.cpp)
target_link_libraries(ShaderCompiler PUBLIC spirv-reflect-static shaderc_combined lz4_static Sqlite3 SQLiteCpp)