# DragonfireEngine
A 3d rendering engine based on the Vulkan API.

Demonstrates the use of indirect rending to preform frustum culling directly on the GPU with compute shaders.

example model from https://graphics.stanford.edu/data/3Dscanrep/

## Building
Be sure to clone recursively to include git submodules

Build with the cmake

```cmake --build --target dragonfire_engine --preset debug```