#pragma once

#include "VulkanDevice.hpp"
#include "VulkanPipelineLayout.hpp"
#include "VulkanShader.hpp"

namespace PathTracer {

    class VulkanPipeline
    {
    public:
        ~VulkanPipeline();

        inline VkPipeline GetPipeline() const { return m_Pipeline; }
    
    protected:
        VulkanPipeline(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanPipelineLayout> layout);

    protected:
        std::shared_ptr<VulkanDevice> m_Device;
        std::shared_ptr<VulkanPipelineLayout> m_Layout;

        VkPipeline m_Pipeline { VK_NULL_HANDLE };
    };

    class VulkanRayTracingPipeline final : public VulkanPipeline
    {
    public:
        class Builder
        {
        public:
            Builder(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanPipelineLayout> layout);

            Builder& AddRayGenShaderGroup(std::shared_ptr<VulkanShader> shader);
            Builder& AddMissShaderGroup(std::shared_ptr<VulkanShader> shader);
            Builder& AddHitShaderGroup(std::shared_ptr<VulkanShader> closestHit, std::shared_ptr<VulkanShader> anyHit = nullptr, std::shared_ptr<VulkanShader> intersection = nullptr);
            Builder& SetMaxDepth(u32 depth);

            std::shared_ptr<VulkanRayTracingPipeline> Build();

        private:
            u32 AddShader(std::shared_ptr<VulkanShader> shader);

        private:
            std::shared_ptr<VulkanDevice> m_Device;
            std::shared_ptr<VulkanPipelineLayout> m_Layout;

            std::vector<std::shared_ptr<VulkanShader>> m_Shaders;
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_Groups;
            u32 m_Depth { 1 };
        };

    public:
        VulkanRayTracingPipeline(
            std::shared_ptr<VulkanDevice> device,
            std::shared_ptr<VulkanPipelineLayout> layout,
            const std::vector<std::shared_ptr<VulkanShader>>& shaders,
            const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
            u32 maxDepth
        );

        virtual ~VulkanRayTracingPipeline() = default;

    private:
    };

}
