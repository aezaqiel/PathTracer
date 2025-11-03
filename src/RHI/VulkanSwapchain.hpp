#pragma once

#include "VulkanDevice.hpp"

namespace PathTracer {

    class VulkanSwapchain
    {
    public:
        VulkanSwapchain(std::shared_ptr<VulkanContext> context, std::shared_ptr<VulkanDevice> device);
        ~VulkanSwapchain();

        inline VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
        inline VkFormat GetFormat() const { return m_Format; }
        inline VkExtent2D GetExtent() const { return m_Extent; }
        inline u32 GetImageCount() const { return m_ImageCount; }

        inline VkImage GetImage(u32 index) const
        {
            return m_Images.at(index);
        }

        inline VkImageView GetImageView(u32 index) const
        {
            return m_ImageViews.at(index);
        }

        void Recreate(u32 width, u32 height);

        VkResult AcquireNextImage(VkSemaphore signalSemaphore, u32& imageIndex);
        VkResult Present(VkQueue presentQueue, std::span<VkSemaphore> waitSemaphores, u32 imageIndex, VkFence signalFence);

    private:
        void Create(u32 width, u32 height);
        void Destroy();

        VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
        VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
        VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height);

    private:
        std::shared_ptr<VulkanContext> m_Context;
        std::shared_ptr<VulkanDevice> m_Device;

        VkSwapchainKHR m_Swapchain { VK_NULL_HANDLE };
        VkFormat m_Format;
        VkExtent2D m_Extent;

        u32 m_ImageCount { 0 };
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;
    };

}
