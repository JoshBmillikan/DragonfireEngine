#version 460
#include "textures.glsl"

layout (location=0) out vec4 outColor;
layout (location=0) in vec3 normal;
layout (location=1) in vec2 uv;
layout (location=2) in vec3 fragPos;
layout (location=3) in flat uint instanceIndex;

layout (set=0, binding=0) uniform Ubo {
    mat4 perspective;
    mat4 orthographic;
    mat4 view;
    vec3 sunDirection;
    vec3 cameraPosition;
    vec2 resolution;
}uboData;

layout(std430, set=1, binding=1) buffer TextureData {
    TextureIndices indices[];
}textureData;

layout(set=1, binding=2) uniform sampler2D bindless_textures[];

void main() {
    vec3 lightColor =  vec3(1.0, 1.0, 1.0);
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;
    vec3 lightDir = normalize(-uboData.sunDirection);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.4;
    vec3 viewDir = normalize(uboData.cameraPosition - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 objectColor = vec3(0.9, 0.7, 0.7);
    TextureIndices textureIndices = textureData.indices[instanceIndex];
    if (textureIndices.base != 0) {
        objectColor = vec3(texture(bindless_textures[textureIndices.base], uv));
    }
    outColor = vec4((ambient + diffuse + specular) * objectColor, 1.0);
}
