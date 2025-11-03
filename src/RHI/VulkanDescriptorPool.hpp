#pragma once

#include "VulkanDevice.hpp"

namespace PathTracer {

    class VulkanDescriptorPool
    {
    public:
        VulkanDescriptorPool(std::shared_ptr<VulkanDevice> device, u32 maxSets, const std::span<VkDescriptorPoolSize>& poolSizes);
        ~VulkanDescriptorPool();

        void Reset();
        std::expected<VkDescriptorSet, VkResult> Allocate(const VkDescriptorSetLayout& layout) const;

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        VkDescriptorPool m_Pool { VK_NULL_HANDLE };
    };

}
