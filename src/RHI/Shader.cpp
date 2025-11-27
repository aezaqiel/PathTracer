#include "Shader.hpp"

#include "Device.hpp"

namespace RHI {

    namespace {

        VkShaderStageFlagBits ToVkStage(Shader::Stage stage)
        {
            switch (stage) {
                case Shader::Stage::Vertex:
                    return VK_SHADER_STAGE_VERTEX_BIT;
                case Shader::Stage::Fragment:
                    return VK_SHADER_STAGE_FRAGMENT_BIT;
                case Shader::Stage::Compute:
                    return VK_SHADER_STAGE_COMPUTE_BIT;
                case Shader::Stage::RayGen:
                    return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
                case Shader::Stage::Miss:
                    return VK_SHADER_STAGE_MISS_BIT_KHR;
                case Shader::Stage::ClosestHit:
                    return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
                case Shader::Stage::AnyHit:
                    return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
                case Shader::Stage::Intersection:
                    return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            }
        }

    }

    Shader::Shader(const std::shared_ptr<Device>& device, const std::filesystem::path& path, Stage stage, const std::string& entry)
        : m_Device(device), m_Stage(stage), m_EntryPoint(entry)
    {
        auto src = ReadFile(path);

        VkShaderModuleCreateInfo shaderInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = src.size(),
            .pCode = reinterpret_cast<const u32*>(src.data())
        };

        VK_CHECK(vkCreateShaderModule(device->GetDevice(), &shaderInfo, nullptr, &m_Module));
    }

    Shader::~Shader()
    {
        vkDestroyShaderModule(m_Device->GetDevice(), m_Module, nullptr);
    }

    VkShaderStageFlagBits Shader::GetStage() const
    {
        return ToVkStage(m_Stage);
    }

    std::vector<char> Shader::ReadFile(const std::filesystem::path& path)
    {
        std::vector<char> src;

        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open {}", path.string());
            return src;
        }

        usize size = file.tellg();
        src.resize(size);

        file.seekg(SEEK_SET);
        file.read(src.data(), size);
        file.close();

        return src;
    }

}
