#pragma once

#include "Core/Types.hpp"
#include "VulkanContext.hpp"

#include <vk_mem_alloc.h>

namespace PathTracer {

    class VulkanDevice
    {
    public:
        VulkanDevice(std::shared_ptr<VulkanContext> ctx);
        ~VulkanDevice();

        inline VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetRtProps() const { return m_RtProps; }
        inline VkDevice GetDevice() const { return m_Device; }

        inline u32 GetQueueFamilyIndex() const { return m_QueueFamilyIndex; }
        inline VkQueue GetQueueFamily() const { return m_QueueFamily; }

        inline VmaAllocator GetAllocator() const { return m_Allocator; }

    private:
        std::shared_ptr<VulkanContext> m_Context;

        VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RtProps;

        VkDevice m_Device { VK_NULL_HANDLE };

        u32 m_QueueFamilyIndex { 0 };
        VkQueue m_QueueFamily { VK_NULL_HANDLE };

        VmaAllocator m_Allocator { VK_NULL_HANDLE };
    };

}
