# Dragonfire Engine

A C++ 20 game engine

## Setup ##

Built and tested with Clang, though other C++ 20 compilers should work.
All dependencies aside from the vulkan sdk are included as git submodules and will be built from source alongside this
project.

1. Install the [Vulkan SDK](https://vulkan.lunarg.com/)
    * Windows: Ensure the VULKAN_SDK environment variable is correctly set as the path to the sdk
2. clone this repo

```
git clone --recursive https://github.com/JoshBmillikan/DragonfireEngine.git
```

4. build with cmake
