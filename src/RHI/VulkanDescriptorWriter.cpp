#include "VulkanDescriptorWriter.hpp"

namespace PathTracer {

    VulkanDescriptorWriter::VulkanDescriptorWriter(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanDescriptorSetLayout> layout, std::shared_ptr<VulkanDescriptorPool> pool)
        : m_Device(device), m_Layout(layout), m_Pool(pool)
    {
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::WriteAccelerationStructure(u32 binding, const std::shared_ptr<VulkanAccelerationStructure>& as)
    {
        VkAccelerationStructureKHR& pAS = m_ASHandles.emplace_back(as->GetAS());
        VkWriteDescriptorSetAccelerationStructureKHR& asInfo = m_ASInfos.emplace_back(VkWriteDescriptorSetAccelerationStructureKHR {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &pAS
        });

        m_Writes.push_back(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &asInfo,
            .dstSet = VK_NULL_HANDLE,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_Layout->GetBinding(binding).descriptorType,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::WriteBuffer(u32 binding, const std::shared_ptr<VulkanBuffer>& buffer, VkDeviceSize offset)
    {
        VkDescriptorBufferInfo& bufferInfo = m_BufferInfos.emplace_back(VkDescriptorBufferInfo {
            .buffer = buffer->GetBuffer(),
            .offset = offset,
            .range = buffer->GetSize()
        });

        m_Writes.push_back(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VK_NULL_HANDLE,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_Layout->GetBinding(binding).descriptorType,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::WriteImage(u32 binding, const std::shared_ptr<VulkanImage>& image, VkImageLayout layout, VkSampler sampler)
    {
        VkDescriptorImageInfo& imageInfo = m_ImageInfos.emplace_back(VkDescriptorImageInfo {
            .sampler = sampler,
            .imageView = image->GetView(),
            .imageLayout = layout
        });

        m_Writes.push_back(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VK_NULL_HANDLE,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_Layout->GetBinding(binding).descriptorType,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    std::expected<VkDescriptorSet, VkResult> VulkanDescriptorWriter::Build()
    {
        auto allocation = m_Pool->Allocate(m_Layout->GetLayout());
        if (!allocation) {
            return std::unexpected(allocation.error());
        }

        VkDescriptorSet set = allocation.value();

        for (auto& write : m_Writes) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(m_Device->GetDevice(), static_cast<u32>(m_Writes.size()), m_Writes.data(), 0, nullptr);

        return set;
    }

}
