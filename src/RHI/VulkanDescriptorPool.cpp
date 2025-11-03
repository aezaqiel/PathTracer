#include "VulkanDescriptorPool.hpp"

namespace PathTracer {

    VulkanDescriptorPool::VulkanDescriptorPool(std::shared_ptr<VulkanDevice> device, u32 maxSets, const std::span<VkDescriptorPoolSize>& poolSizes)
        : m_Device(device)
    {
        VkDescriptorPoolCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = maxSets,
            .poolSizeCount = static_cast<u32>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };
        VK_CHECK(vkCreateDescriptorPool(m_Device->GetDevice(), &createInfo, nullptr, &m_Pool));
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        vkDestroyDescriptorPool(m_Device->GetDevice(), m_Pool, nullptr);
    }

    void VulkanDescriptorPool::Reset()
    {
        vkResetDescriptorPool(m_Device->GetDevice(), m_Pool, 0);
    }

    std::expected<VkDescriptorSet, VkResult> VulkanDescriptorPool::Allocate(const VkDescriptorSetLayout& layout) const
    {
        VkDescriptorSetAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_Pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout
        };

        VkDescriptorSet set = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(m_Device->GetDevice(), &allocateInfo, &set);

        if (set == VK_NULL_HANDLE) {
            return std::unexpected(result);
        }

        return set;
    }

}
