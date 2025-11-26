#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Instance;

    enum class QueueType : u8
    {
        Graphics,
        Compute,
        Transfer
    };

    class Device
    {
    public:
        Device(const std::shared_ptr<Instance>& instance);
        ~Device();

        inline VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        inline VkDevice GetDevice() const { return m_Device; }
        inline VmaAllocator GetAllocator() const { return m_Allocator; }

        inline VkPhysicalDeviceProperties2 GetProps() const { return m_Props; }
        inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetRTProps() const { return m_RTProps; }

        inline void WaitIdle() const { vkDeviceWaitIdle(m_Device); }

        template <QueueType type>
        inline constexpr VkQueue GetQueue() const
        {
            if constexpr (type == QueueType::Graphics) return m_GraphicsQueue;
            if constexpr (type == QueueType::Compute) return m_ComputeQueue;
            if constexpr (type == QueueType::Transfer) return m_TransferQueue;

            LOG_WARN("Unhandled QueueType for Device::GetQueue");
            return VK_NULL_HANDLE;
        }

        template <QueueType type>
        inline constexpr u32 GetQueueFamily() const
        {
            if constexpr (type == QueueType::Graphics) return m_QueueFamily.graphics.value();
            if constexpr (type == QueueType::Compute) return m_QueueFamily.compute.value();
            if constexpr (type == QueueType::Transfer) return m_QueueFamily.transfer.value();

            LOG_WARN("Unhandled QueueType for Device::GetQueueFamily");
            return 0;
        }

        inline static constexpr u32 GetFrameInFlight() { return s_FrameInFlight; }

    private:
        struct QueueFamilyIndices
        {
            std::optional<u32> graphics;
            std::optional<u32> compute;
            std::optional<u32> transfer;

            inline bool IsComplete() const
            {
                return graphics.has_value() && compute.has_value() && transfer.has_value();
            }

            inline std::set<u32> UniqueFamilies() const
            {
                return std::set<u32> {
                    graphics.value(),
                    compute.value(),
                    transfer.value()
                };
            }
        };

    private:
        void SelectPhysicalDevice();
        void CreateDevice();

        static i32 ScorePhysicalDevice(VkPhysicalDevice device);
        static QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice device);

    private:
        inline static constexpr usize s_FrameInFlight { 3 };

    private:
        std::shared_ptr<Instance> m_Instance;

        VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
        VkDevice m_Device { VK_NULL_HANDLE };
        VmaAllocator m_Allocator { VK_NULL_HANDLE };

        VkPhysicalDeviceProperties2 m_Props;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RTProps;

        QueueFamilyIndices m_QueueFamily;
        VkQueue m_GraphicsQueue { VK_NULL_HANDLE };
        VkQueue m_ComputeQueue { VK_NULL_HANDLE };
        VkQueue m_TransferQueue { VK_NULL_HANDLE };
    };

}
