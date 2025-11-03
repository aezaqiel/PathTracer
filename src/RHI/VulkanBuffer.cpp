#include "VulkanBuffer.hpp"

namespace PathTracer {

    VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanDevice> device, const Spec& spec)
        : m_Device(device), m_Size(spec.size)
    {
        VkBufferCreateInfo bufferInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = m_Size,
            .usage = spec.usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        VK_CHECK(vkCreateBuffer(m_Device->GetDevice(), &bufferInfo, nullptr, &m_Buffer));

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(m_Device->GetDevice(), m_Buffer, &memoryRequirements);

        VkMemoryAllocateFlagsInfo allocateFlags {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .pNext = nullptr,
            .flags = 0,
            .deviceMask = 0
        };

        VkMemoryAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &allocateFlags,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = FindMemoryType(m_Device->GetPhysicalDevice(), memoryRequirements.memoryTypeBits, spec.memoryProperties)
        };

        if (spec.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            allocateFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        }

        VK_CHECK(vkAllocateMemory(m_Device->GetDevice(), &allocateInfo, nullptr, &m_Memory));
        VK_CHECK(vkBindBufferMemory(m_Device->GetDevice(), m_Buffer, m_Memory, 0));

        if (spec.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            VkBufferDeviceAddressInfo addressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = nullptr,
                .buffer = m_Buffer
            };

            m_DeviceAddress = vkGetBufferDeviceAddress(m_Device->GetDevice(), &addressInfo);
        }
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_MappedData) Unmap();

        vkDestroyBuffer(m_Device->GetDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Device->GetDevice(), m_Memory, nullptr);
    }

    void VulkanBuffer::Write(void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        Map(size, offset);
        memcpy(m_MappedData, data, static_cast<usize>(size));
        Unmap();
    }

    void* VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset)
    {
        if (m_MappedData) {
            std::println("Buffer memory already mapped, unmapping old data...");
            Unmap();
        }

        VK_CHECK(vkMapMemory(m_Device->GetDevice(), m_Memory, offset, size, 0, &m_MappedData));

        return m_MappedData;
    }

    void VulkanBuffer::Unmap()
    {
        if (m_MappedData != nullptr) {
            vkUnmapMemory(m_Device->GetDevice(), m_Memory);
            m_MappedData = nullptr;
        } else {
            std::println("No memory mapped to buffer");
        }
    }

}
