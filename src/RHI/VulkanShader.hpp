#pragma once

#include "VulkanDevice.hpp"

namespace PathTracer {

    class VulkanShader
    {
    public:
        VulkanShader(std::shared_ptr<VulkanDevice> device, VkShaderStageFlagBits stage, const std::string& filename);
        ~VulkanShader();

        inline VkShaderModule GetModule() const { return m_Module; }
        inline VkShaderStageFlagBits GetStage() const { return m_Stage; }

    private:
        static std::vector<char> ReadFile(const std::string& filepath);

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        VkShaderModule m_Module { VK_NULL_HANDLE };
        VkShaderStageFlagBits m_Stage;
    };

}
