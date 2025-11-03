#include "VulkanPipeline.hpp"

namespace PathTracer {

    VulkanPipeline::VulkanPipeline(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanPipelineLayout> layout)
        : m_Device(device), m_Layout(layout)
    {
    }

    VulkanPipeline::~VulkanPipeline()
    {
        vkDestroyPipeline(m_Device->GetDevice(), m_Pipeline, nullptr);
    }

    VulkanRayTracingPipeline::Builder::Builder(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanPipelineLayout> layout)
        : m_Device(device), m_Layout(layout)
    {
    }

    VulkanRayTracingPipeline::Builder& VulkanRayTracingPipeline::Builder::AddRayGenShaderGroup(std::shared_ptr<VulkanShader> shader)
    {
        m_Groups.push_back(VkRayTracingShaderGroupCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = AddShader(shader),
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        });

        return *this;
    }

    VulkanRayTracingPipeline::Builder& VulkanRayTracingPipeline::Builder::AddMissShaderGroup(std::shared_ptr<VulkanShader> shader)
    {
        m_Groups.push_back(VkRayTracingShaderGroupCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = AddShader(shader),
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        });

        return *this;
    }

    VulkanRayTracingPipeline::Builder& VulkanRayTracingPipeline::Builder::AddHitShaderGroup(std::shared_ptr<VulkanShader> closestHit, std::shared_ptr<VulkanShader> anyHit, std::shared_ptr<VulkanShader> intersection)
    {
        m_Groups.push_back(VkRayTracingShaderGroupCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = intersection ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = AddShader(closestHit),
            .anyHitShader = AddShader(anyHit),
            .intersectionShader = AddShader(intersection),
            .pShaderGroupCaptureReplayHandle = nullptr
        });

        return *this;
    }

    VulkanRayTracingPipeline::Builder& VulkanRayTracingPipeline::Builder::SetMaxDepth(u32 depth)
    {
        m_Depth = depth;
        return *this;
    }

    std::shared_ptr<VulkanRayTracingPipeline> VulkanRayTracingPipeline::Builder::Build()
    {
        return std::make_shared<VulkanRayTracingPipeline>(m_Device, m_Layout, m_Shaders, m_Groups, m_Depth);
    }

    u32 VulkanRayTracingPipeline::Builder::AddShader(std::shared_ptr<VulkanShader> shader)
    {
        if (!shader) {
            return VK_SHADER_UNUSED_KHR;
        }

        u32 index = static_cast<u32>(m_Shaders.size());
        m_Shaders.push_back(shader);

        return index;
    }

    VulkanRayTracingPipeline::VulkanRayTracingPipeline(
        std::shared_ptr<VulkanDevice> device,
        std::shared_ptr<VulkanPipelineLayout> layout,
        const std::vector<std::shared_ptr<VulkanShader>>& shaders,
        const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
        u32 maxDepth
    )
        : VulkanPipeline(device, layout)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.reserve(shaders.size());

        for (const auto& shader : shaders) {
            shaderStages.push_back(VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = shader->GetStage(),
                .module = shader->GetModule(),
                .pName = "main",
                .pSpecializationInfo = nullptr
            });
        }

        VkRayTracingPipelineCreateInfoKHR createInfo {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = static_cast<u32>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .groupCount = static_cast<u32>(groups.size()),
            .pGroups = groups.data(),
            .maxPipelineRayRecursionDepth = maxDepth,
            .pLibraryInfo = nullptr,
            .pLibraryInterface = nullptr,
            .pDynamicState = nullptr,
            .layout = m_Layout->GetLayout(),
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VK_CHECK(vkCreateRayTracingPipelinesKHR(m_Device->GetDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_Pipeline));
    }

}
