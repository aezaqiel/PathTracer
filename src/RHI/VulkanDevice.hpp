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
        inline VkPhysicalDeviceProperties2 GetProps() const { return m_Props; }
        inline VkDevice GetDevice() const { return m_Device; }

        // TODO: Remove
        inline u32 GetQueueFamilyIndex() const { return m_QueueIndices.compute.value(); }
        inline VkQueue GetQueueFamily() const { return m_ComputeQueue; }

        inline VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        inline u32 GetGraphicsQueueIndex() const { return m_QueueIndices.graphics.value(); }

        inline VkQueue GetPresentQueue() const { return m_PresentQueue; }
        inline u32 GetPresentQueueIndex() const { return m_QueueIndices.present.value(); }

        inline VkQueue GetComputeQueue() const { return m_ComputeQueue; }
        inline u32 GetComputeQueueIndex() const { return m_QueueIndices.compute.value(); }

        inline VkQueue GetTransferQueue() const { return m_TransferQueue; }
        inline u32 GetTransferQueueIndex() const { return m_QueueIndices.transfer.value(); }

        inline VmaAllocator GetAllocator() const { return m_Allocator; }

    private:
        struct QueueFamilyIndices
        {
            std::optional<u32> graphics;
            std::optional<u32> present;
            std::optional<u32> compute;
            std::optional<u32> transfer;

            inline bool IsSufficient() const
            {
                return graphics.has_value() && present.has_value();
            }

            inline std::set<u32> GetUniqueIndices() const
            {
                return std::set<u32> { graphics.value(), present.value(), compute.value(), transfer.value() };
            }
        };

    private:
        void PickPhysicalDevice();

        static bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions);
        static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    private:
        std::shared_ptr<VulkanContext> m_Context;

        VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RtProps;
        VkPhysicalDeviceProperties2 m_Props;

        QueueFamilyIndices m_QueueIndices;

        VkDevice m_Device { VK_NULL_HANDLE };

        VkQueue m_GraphicsQueue;
        VkQueue m_PresentQueue;
        VkQueue m_ComputeQueue;
        VkQueue m_TransferQueue;

        VmaAllocator m_Allocator { VK_NULL_HANDLE };
    };

}
