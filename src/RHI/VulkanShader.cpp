#include "VulkanShader.hpp"

#include "Core/Logger.hpp"

namespace PathTracer {

    VulkanShader::VulkanShader(std::shared_ptr<VulkanDevice> device, VkShaderStageFlagBits stage, const std::string& filename)
        : m_Device(device), m_Stage(stage)
    {
        std::vector<char> src = ReadFile(std::format("{}{}", SHADER_DIR, filename));

        VkShaderModuleCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = src.size(),
            .pCode = reinterpret_cast<const u32*>(src.data())
        };

        VK_CHECK(vkCreateShaderModule(m_Device->GetDevice(), &createInfo, nullptr, &m_Module));
    }

    VulkanShader::~VulkanShader()
    {
        vkDestroyShaderModule(m_Device->GetDevice(), m_Module, nullptr);
    }

    std::vector<char> VulkanShader::ReadFile(const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file {}", filepath);
            std::abort();
        }

        usize size = file.tellg();
        std::vector<char> buffer(size);

        file.seekg(SEEK_SET);
        file.read(buffer.data(), size);
        file.close();

        return buffer;
    }

}
