#include "Buffer.hpp"

#include "Device.hpp"

namespace RHI {

    Buffer::Buffer(const Device& device, const Spec& spec)
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

        VmaAllocationCreateInfo allocationInfo;
        memset(&allocationInfo, 0, sizeof(VmaAllocationCreateInfo));
        allocationInfo.usage = spec.memory;

        if (spec.memory == VMA_MEMORY_USAGE_AUTO_PREFER_HOST || spec.memory == VMA_MEMORY_USAGE_CPU_TO_GPU || spec.memory == VMA_MEMORY_USAGE_GPU_TO_CPU) {
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VK_CHECK(vmaCreateBuffer(m_Device.GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));

        if (spec.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            VkBufferDeviceAddressInfo addressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = nullptr,
                .buffer = m_Buffer
            };

            m_DeviceAddress = vkGetBufferDeviceAddress(m_Device.GetDevice(), &addressInfo);
        }
    }

    Buffer::~Buffer()
    {
        if (m_MappedData) Unmap();
        vmaDestroyBuffer(m_Device.GetAllocator(), m_Buffer, m_Allocation);
    }

    void* Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
    {
        if (m_MappedData) {
            LOG_WARN("Buffer memory alreay mapped, unmapping old data...");
            Unmap();
        }

        VK_CHECK(vmaMapMemory(m_Device.GetAllocator(), m_Allocation, &m_MappedData));

        return reinterpret_cast<void*>(reinterpret_cast<u8*>(m_MappedData) + offset);
    }

    void Buffer::Unmap()
    {
        if (!m_MappedData) {
            LOG_WARN("No memory mapped to buffer");
            return;
        }

        vmaUnmapMemory(m_Device.GetAllocator(), m_Allocation);
        m_MappedData = nullptr;
    }

    void Buffer::Write(void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        Map(size, offset);
        memcpy(m_MappedData, data, static_cast<usize>(size));
        Unmap();
    }

}
