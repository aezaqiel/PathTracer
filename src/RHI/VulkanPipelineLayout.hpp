#pragma once

#include "VulkanDevice.hpp"
#include "VulkanDescriptorSetLayout.hpp"

namespace PathTracer {

    class VulkanPipelineLayout
    {
    public:
        class Builder
        {
        public:
            Builder(std::shared_ptr<VulkanDevice> device);

            Builder& AddSetLayout(std::shared_ptr<VulkanDescriptorSetLayout> layout);
            Builder& AddPushConstant(VkPushConstantRange pushContant);

            std::shared_ptr<VulkanPipelineLayout> Build();

        private:
            std::shared_ptr<VulkanDevice> m_Device;
            std::vector<std::shared_ptr<VulkanDescriptorSetLayout>> m_SetLayouts;
            std::vector<VkPushConstantRange> m_PushContants;
        };

    public:
        VulkanPipelineLayout(std::shared_ptr<VulkanDevice> device, std::span<VkDescriptorSetLayout> setLayouts, std::span<VkPushConstantRange> pushConstants);
        ~VulkanPipelineLayout();

        inline VkPipelineLayout GetLayout() const { return m_Layout; }

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        VkPipelineLayout m_Layout { VK_NULL_HANDLE };
    };

}
