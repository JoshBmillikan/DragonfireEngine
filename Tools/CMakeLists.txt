
add_executable(SpvToDb SpvToDb.cpp)
target_link_libraries(SpvToDb PUBLIC spirv-reflect-static Sqlite3 SQLiteCpp)