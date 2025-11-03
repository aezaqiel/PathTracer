#pragma once

#include "VulkanDevice.hpp"
#include "Core/Logger.hpp"

namespace PathTracer {

    class VulkanDescriptorSetLayout
    {
    public:
        class Builder
        {
        public:
            Builder(std::shared_ptr<VulkanDevice> device);
            Builder& AddBinding(u32 binding, VkDescriptorType type, VkShaderStageFlags stage, u32 count = 1);

            std::shared_ptr<VulkanDescriptorSetLayout> Build() const;

        private:
            std::shared_ptr<VulkanDevice> m_Device;
            std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
        };

    public:
        VulkanDescriptorSetLayout(std::shared_ptr<VulkanDevice> device, const std::vector<VkDescriptorSetLayoutBinding>& bindings);
        ~VulkanDescriptorSetLayout();

        inline const VkDescriptorSetLayout& GetLayout() const { return m_Layout; }
        inline const VkDescriptorSetLayoutBinding& GetBinding(u32 binding) const
        {
            if (m_Bindings.find(binding) == m_Bindings.end()) {
                LOG_ERROR("Binding not found in descriptor set layout");
            }
            return m_Bindings.at(binding);
        }

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        VkDescriptorSetLayout m_Layout { VK_NULL_HANDLE };
        std::unordered_map<u32, VkDescriptorSetLayoutBinding> m_Bindings;
    };

}
