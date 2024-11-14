# DragonfireEngine
A 3d rendering engine based on the Vulkan API.

Demonstrates the use of indirect rending to preform frustum culling directly on the GPU with compute shaders.

example model from https://graphics.stanford.edu/data/3Dscanrep/

## Building
1. Clone the repository with ```--recursive``` to include git submodules

2. Install [vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-powershell#1---set-up-vcpkg)

3. Build with cmake ```cmake --build --target dragonfire_engine --preset debug```