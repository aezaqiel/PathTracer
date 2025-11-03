#include "VulkanPipelineLayout.hpp"

namespace PathTracer {

    VulkanPipelineLayout::Builder::Builder(std::shared_ptr<VulkanDevice> device)
        : m_Device(device)
    {
    }

    VulkanPipelineLayout::Builder& VulkanPipelineLayout::Builder::AddSetLayout(std::shared_ptr<VulkanDescriptorSetLayout> layout)
    {
        m_SetLayouts.push_back(layout);
        return *this;
    }

    VulkanPipelineLayout::Builder& VulkanPipelineLayout::Builder::AddPushConstant(VkPushConstantRange pushContant)
    {
        m_PushContants.push_back(pushContant);
        return *this;
    }

    std::shared_ptr<VulkanPipelineLayout> VulkanPipelineLayout::Builder::Build()
    {
        std::vector<VkDescriptorSetLayout> setLayouts;
        setLayouts.reserve(m_SetLayouts.size());

        for (const auto& set : m_SetLayouts)
            setLayouts.push_back(set->GetLayout());

        return std::make_shared<VulkanPipelineLayout>(m_Device, setLayouts, m_PushContants);
    }

    VulkanPipelineLayout::VulkanPipelineLayout(std::shared_ptr<VulkanDevice> device, std::span<VkDescriptorSetLayout> setLayouts, std::span<VkPushConstantRange> pushConstants)
        : m_Device(device)
    {
        VkPipelineLayoutCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<u32>(setLayouts.size()),
            .pSetLayouts = setLayouts.data(),
            .pushConstantRangeCount = static_cast<u32>(pushConstants.size()),
            .pPushConstantRanges = pushConstants.data() 
        };

        VK_CHECK(vkCreatePipelineLayout(m_Device->GetDevice(), &createInfo, nullptr, &m_Layout));
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        vkDestroyPipelineLayout(m_Device->GetDevice(), m_Layout, nullptr);
    }

}
