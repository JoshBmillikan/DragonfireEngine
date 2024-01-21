#version 460

layout (location=0) in vec3 position;
layout (location=1) in vec3 normal;
layout (location=2) in vec2 uv;

layout (set=0, binding=0) uniform Ubo {
    mat4 perspective;
    mat4 orthographic;
    mat4 view;
    vec3 sunDirection;
    vec3 cameraPosition;
    vec2 resolution;
}uboData;

layout (std430, set=1, binding=0) readonly buffer Transforms {
    mat4 modelMatrices[];
}transforms;

layout (location=0) out vec3 normalOut;
layout (location=1) out vec2 uvOut;
layout (location=2) out vec3 fragPos;
layout (location=3) out uint instanceIndex;

void main()
{
    mat4 model = transforms.modelMatrices[gl_InstanceIndex];
    mat4 transform = uboData.perspective * model;
    gl_Position = transform * vec4(position, 1.0);
    fragPos = vec3(model * vec4 (position, 1.0));
    // forward normals and uvs to fragment shader
    normalOut = normal;
    uvOut = uv;
    instanceIndex = gl_InstanceIndex;
}
