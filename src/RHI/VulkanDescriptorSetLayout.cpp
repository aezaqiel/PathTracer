#include "VulkanDescriptorSetLayout.hpp"

namespace PathTracer {

    VulkanDescriptorSetLayout::Builder::Builder(std::shared_ptr<VulkanDevice> device)
        : m_Device(device)
    {
    }

    VulkanDescriptorSetLayout::Builder& VulkanDescriptorSetLayout::Builder::AddBinding(u32 binding, VkDescriptorType type, VkShaderStageFlags stage, u32 count)
    {
        m_Bindings.push_back({
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = count,
            .stageFlags = stage,
            .pImmutableSamplers = nullptr
        });
        
        return *this;
    }

    std::shared_ptr<VulkanDescriptorSetLayout> VulkanDescriptorSetLayout::Builder::Build() const
    {
        return std::make_shared<VulkanDescriptorSetLayout>(m_Device, m_Bindings);
    }

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(std::shared_ptr<VulkanDevice> device, const std::vector<VkDescriptorSetLayoutBinding>& bindings)
        : m_Device(device)
    {
        for (const auto& binding : bindings) {
            m_Bindings[binding.binding] = binding;
        }

        VkDescriptorSetLayoutCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<u32>(bindings.size()),
            .pBindings = bindings.data()
        };

        VK_CHECK(vkCreateDescriptorSetLayout(m_Device->GetDevice(), &createInfo, nullptr, &m_Layout));
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_Layout, nullptr);
    }

}
