#pragma once

#include "VulkanDevice.hpp"

namespace PathTracer {

    class VulkanBuffer
    {
    public:
        struct Spec
        {
            VkDeviceSize size;
            VkBufferUsageFlags usage;
            VkMemoryPropertyFlags memoryProperties;
        };

    public:
        VulkanBuffer(std::shared_ptr<VulkanDevice> device, const Spec& spec);
        ~VulkanBuffer();

        void* Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void Unmap();
        void Write(void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        inline VkBuffer GetBuffer() const { return m_Buffer; }
        inline VkDeviceMemory GetMemory() const { return m_Memory; }
        inline VkDeviceSize GetSize() const { return m_Size; }
        inline VkDeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        VkBuffer m_Buffer { VK_NULL_HANDLE };
        VkDeviceMemory m_Memory { VK_NULL_HANDLE };
        VkDeviceSize m_Size { 0 };
        VkDeviceAddress m_DeviceAddress { 0 };

        void* m_MappedData { nullptr };
    };

}
