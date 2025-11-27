#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Device;

    class Shader
    {
    public:
        enum class Stage
        {
            Vertex,
            Fragment,
            Compute,
            RayGen,
            Miss,
            ClosestHit,
            AnyHit,
            Intersection
        };

    public:
        Shader(const std::shared_ptr<Device>& device, const std::filesystem::path& path, Stage stage);
        ~Shader();

        inline VkShaderModule GetModule() const { return m_Module; }
        VkShaderStageFlagBits GetStage() const;

    private:
        static std::vector<char> ReadFile(const std::filesystem::path& path);

    private:
        std::shared_ptr<Device> m_Device;

        VkShaderModule m_Module { VK_NULL_HANDLE };
        Stage m_Stage;
    };

}
