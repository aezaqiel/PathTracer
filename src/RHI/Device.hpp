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

        void SyncFrame();

        inline VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        inline VkDevice GetDevice() const { return m_Device; }
        inline VmaAllocator GetAllocator() const { return m_Allocator; }

        inline VkPhysicalDeviceProperties2 GetProps() const { return m_Props; }
        inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetRTProps() const { return m_RTProps; }
        inline VkPhysicalDeviceAccelerationStructurePropertiesKHR GetASProps() const { return m_ASProps; }

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

        template <QueueType type>
        inline constexpr VkSemaphore GetTimeline() const
        {
            if constexpr (type == QueueType::Graphics) return m_GraphicsTimeline;
            if constexpr (type == QueueType::Compute) return m_ComputeTimeline;
            if constexpr (type == QueueType::Transfer) return m_TransferTimeline;

            LOG_WARN("Unhandled QueueType for Device::GetTimeline");
            return VK_NULL_HANDLE;
        }

        template <QueueType type>
        inline constexpr u64 GetTimelineValue() const
        {
            if constexpr (type == QueueType::Graphics) return m_GraphicsTimelineValue;
            if constexpr (type == QueueType::Compute) return m_ComputeTimelineValue;
            if constexpr (type == QueueType::Transfer) return m_TransferTimelineValue;

            LOG_WARN("Unhandled QueueType for Device::GetTimelineValue");
            return 0;
        }

        template <QueueType type>
        inline constexpr u64 IncrementTimeline()
        {
            if constexpr (type == QueueType::Graphics) return ++m_GraphicsTimelineValue;
            if constexpr (type == QueueType::Compute) return ++m_ComputeTimelineValue;
            if constexpr (type == QueueType::Transfer) return ++m_TransferTimelineValue;

            LOG_WARN("Unhandled QueueType for Device::GetTimelineValue");
            return 0;
        }

        template <QueueType type>
        inline constexpr void SyncTimeline()
        {
            VkSemaphore timeline = GetTimeline<type>();
            u64 value = GetTimelineValue<type>();

            VkSemaphoreWaitInfo wait {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .pNext = nullptr,
                .flags = 0,
                .semaphoreCount = 1,
                .pSemaphores = &timeline,
                .pValues = &value
            };
            VK_CHECK(vkWaitSemaphores(m_Device, &wait, std::numeric_limits<u64>::max()));
        }

        template <QueueType type>
        inline VkSemaphoreSubmitInfo Submit(VkCommandBuffer cmd, const std::span<VkSemaphoreSubmitInfo>& wait, const std::span<VkSemaphoreSubmitInfo>& signal)
        {
            std::vector<VkSemaphoreSubmitInfo> allSignal(signal.begin(), signal.end());
            allSignal.push_back(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = GetTimeline<type>(),
                .value = IncrementTimeline<type>(),
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .deviceIndex = 0
            });

            VkCommandBufferSubmitInfo cmdSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = cmd,
                .deviceMask = 0
            };

            VkSubmitInfo2 submitInfo {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext = nullptr,
                .flags = 0,
                .waitSemaphoreInfoCount = static_cast<u32>(wait.size()),
                .pWaitSemaphoreInfos = wait.data(),
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &cmdSubmitInfo,
                .signalSemaphoreInfoCount = static_cast<u32>(allSignal.size()),
                .pSignalSemaphoreInfos = allSignal.data()
            };

            VK_CHECK(vkQueueSubmit2(GetQueue<type>(), 1, &submitInfo, VK_NULL_HANDLE));

            return allSignal.back();
        }

        inline usize GetCurrentFrameIndex() const { return m_CurrentFrameIndex; }
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
        void CreateSyncObjects();

        i32 ScorePhysicalDevice(VkPhysicalDevice device);
        QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice device);

    private:
        inline static constexpr usize s_FrameInFlight { 3 };

    private:
        std::shared_ptr<Instance> m_Instance;

        VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
        VkDevice m_Device { VK_NULL_HANDLE };
        VmaAllocator m_Allocator { VK_NULL_HANDLE };

        VkPhysicalDeviceProperties2 m_Props;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RTProps;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR m_ASProps;

        QueueFamilyIndices m_QueueFamily;
        VkQueue m_GraphicsQueue { VK_NULL_HANDLE };
        VkQueue m_ComputeQueue { VK_NULL_HANDLE };
        VkQueue m_TransferQueue { VK_NULL_HANDLE };

        usize m_CurrentFrameIndex { 0 };
        u64 m_HostFrameIndex { 0 };
        u64 m_LocalFrameIndex { 0 };

        VkSemaphore m_GraphicsTimeline { VK_NULL_HANDLE };
        VkSemaphore m_ComputeTimeline { VK_NULL_HANDLE };
        VkSemaphore m_TransferTimeline { VK_NULL_HANDLE };

        u64 m_GraphicsTimelineValue { 0 };
        u64 m_ComputeTimelineValue { 0 };
        u64 m_TransferTimelineValue { 0 };
    };

}
