#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Device;

    class Buffer
    {
    public:
        struct Spec
        {
            VkDeviceSize size;
            VkBufferUsageFlags usage;
            VmaMemoryUsage memory;
        };

    public:
        Buffer(const std::shared_ptr<Device>& device, const Spec& spec);
        ~Buffer();

        inline VkBuffer GetBuffer() const { return m_Buffer; }
        inline VkDeviceSize GetSize() const { return m_Size; }
        inline VkDeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }

        void* Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void Unmap();

        void Write(void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    private:
        std::shared_ptr<Device> m_Device;

        VkBuffer m_Buffer { VK_NULL_HANDLE };
        VmaAllocation m_Allocation { VK_NULL_HANDLE };

        VkDeviceSize m_Size { 0 };
        VkDeviceAddress m_DeviceAddress { 0 };

        void* m_MappedData { nullptr };
    };

}
