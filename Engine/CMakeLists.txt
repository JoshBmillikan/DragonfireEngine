
add_library(DragonfireEngine SHARED Include/Engine.h Source/Engine.cpp Source/Service.cpp Include/Service.h)
target_include_directories(DragonfireEngine PUBLIC Include)
target_compile_definitions(DragonfireEngine PRIVATE _DLL)

target_precompile_headers(DragonfireEngine PUBLIC <vector> <algorithm> <memory> <stdexcept>)