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
            vk::ShaderModule module;
            vk::ShaderStageFlagBits flags{};
        };
        class Loader {
            vk::PipelineCache cache;
            vk::Device device;
            std::unordered_map<std::string, std::unique_ptr<Shader>> loadedShaders;
        public:
            Loader(vk::Device device);
            ~Loader() noexcept;
            Shader* getShader(const std::string& shaderName);
        };
    };

}   // namespace dragonfire
