//
// Created by josh on 5/21/22.
//

#pragma once

namespace dragonfire::rendering {

class Shader {
    vk::PipelineLayout layout;
    std::array<vk::DescriptorSetLayout, 4> descriptorLayouts;

public:

    struct Module {
        vk::UniqueShaderModule module;
        vk::ShaderStageFlagBits flags{};
        Module(vk::UniqueShaderModule&& module, vk::ShaderStageFlagBits flags) : module(std::move(module)), flags(flags) {}
    };
    class Library {
        vk::PipelineCache cache;
        vk::Device device;
        std::unordered_map<std::string, vk::UniqueShaderModule> loadedShaders;

    public:
        Library(vk::Device device);
        ~Library() noexcept;
        vk::ShaderModule& getShader(const std::string& shaderName);
        vk::ShaderModule& operator [](const std::string& shaderName) {
            return getShader(shaderName);
        }
    };

    Shader(std::string_view info, std::vector<Module>&& modules);

private:
    std::vector<Module> modules;
};

}   // namespace dragonfire::rendering
