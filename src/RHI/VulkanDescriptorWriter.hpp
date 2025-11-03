#pragma once

#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanAccelerationStructure.hpp"
#include "VulkanImage.hpp"
#include "VulkanDescriptorSetLayout.hpp"
#include "VulkanDescriptorPool.hpp"

namespace PathTracer {

    class VulkanDescriptorWriter
    {
    public:
        VulkanDescriptorWriter(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanDescriptorSetLayout> layout, std::shared_ptr<VulkanDescriptorPool> pool);
        ~VulkanDescriptorWriter() = default;

        VulkanDescriptorWriter& WriteAccelerationStructure(u32 binding, const std::shared_ptr<VulkanAccelerationStructure>& as);
        VulkanDescriptorWriter& WriteBuffer(u32 binding, const std::shared_ptr<VulkanBuffer>& buffer, VkDeviceSize offset);
        VulkanDescriptorWriter& WriteImage(u32 binding, const std::shared_ptr<VulkanImage>& image, VkImageLayout layout, VkSampler sampler);

        std::expected<VkDescriptorSet, VkResult> Build();

    private:
        std::shared_ptr<VulkanDevice> m_Device;
        std::shared_ptr<VulkanDescriptorSetLayout> m_Layout;
        std::shared_ptr<VulkanDescriptorPool> m_Pool;

        std::vector<VkWriteDescriptorSet> m_Writes;

        std::deque<VkAccelerationStructureKHR> m_ASHandles;
        std::deque<VkWriteDescriptorSetAccelerationStructureKHR> m_ASInfos;

        std::deque<VkDescriptorBufferInfo> m_BufferInfos;
        std::deque<VkDescriptorImageInfo> m_ImageInfos;
    };

}
