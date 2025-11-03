#pragma once

#include "VulkanDevice.hpp"
#include "VulkanSwapchain.hpp"

namespace PathTracer {

    enum class QueueType
    {
        Graphics,
        Transfer,
        Compute
    };

    struct FrameContext
    {
        VkCommandBuffer graphicsBuffer;
        VkCommandBuffer transferBuffer;
        VkCommandBuffer computeBuffer;

        std::unordered_map<QueueType, VkCommandBuffer> buffers;
        u32 imageIndex;
        u32 frameIndex;
    };

    class VulkanCommandManager
    {
    public:
        VulkanCommandManager(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanSwapchain> swapchain);
        ~VulkanCommandManager();

        void SubmitOnce(QueueType queue, std::function<void(VkCommandBuffer)>&& func);

        std::optional<FrameContext> BeginFrame();
        void EndFrame();

        inline void WaitIdle() { vkDeviceWaitIdle(m_Device->GetDevice()); }

        inline static constexpr u32 GetFrameInFlight() { return s_FrameInFlight; }

    private:
        struct FrameData
        {
            VkFence inFlightFence;

            VkSemaphore imageAvailableSemaphore;
            VkSemaphore renderFinishedSemaphore;
            VkSemaphore transferFinishedSemaphore;
            VkSemaphore computeFinishedSemaphore;

            std::unordered_map<QueueType, std::pair<VkCommandPool, VkCommandBuffer>> commands;

            VkCommandPool graphicsPool { VK_NULL_HANDLE };
            VkCommandBuffer graphicsBuffer { VK_NULL_HANDLE };

            VkCommandPool transferPool { VK_NULL_HANDLE };
            VkCommandBuffer transferBuffer { VK_NULL_HANDLE };

            VkCommandPool computePool { VK_NULL_HANDLE };
            VkCommandBuffer computeBuffer { VK_NULL_HANDLE };
        };

        void CreateFrameData();
        void DestroyFrameData();

        void CreateSubmitOnceData();
        void DestroySubmitOnceData();

    private:
        std::shared_ptr<VulkanDevice> m_Device;
        std::shared_ptr<VulkanSwapchain> m_Swapchain;

        inline static constexpr u32 s_FrameInFlight { 3 };

        std::array<FrameData, s_FrameInFlight> m_Frames;
        bool m_FrameInProgress { false };

        u32 m_CurrentImage { 0 };
        u32 m_CurrentFrame { 0 };

        std::mutex m_SubmitOnceMutex;
        VkFence m_SubmitOnceFence { VK_NULL_HANDLE };

        std::unordered_map<QueueType, VkCommandPool> m_SubmitOncePools;
    };

}
