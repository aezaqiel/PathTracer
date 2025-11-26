#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Instance;
    class Device;

    class Swapchain
    {
    public:
        Swapchain(const std::shared_ptr<Instance>& instance, const std::shared_ptr<Device>& device);
        ~Swapchain();

        inline VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
        inline VkFormat GetFormat() const { return m_Format; }
        inline VkExtent2D GetExtent() const { return m_Extent; }
        inline u32 GetImageCount() const { return m_ImageCount; }

        inline VkImage GetCurrentImage() const
        {
            return m_Images[m_CurrentImageIndex];
        }

        inline VkImageView GetCurrentImageView() const
        {
            return m_ImageViews[m_CurrentImageIndex];
        }

        inline VkSemaphore GetCurrentImageSemaphore() const
        {
            return m_ImageAvailableSemaphores[m_CurrentSyncIndex];
        }

        inline VkSemaphore GetPresentSignalSemaphore() const
        {
            return m_PresentSemaphores[m_CurrentSyncIndex];
        }

        inline u32 GetCurrentImageIndex() const { return m_CurrentImageIndex; }

        void Create(u32 width, u32 height);

        std::optional<VkResult> AcquireNextImage();
        std::optional<VkResult> Present();

        VkSemaphoreSubmitInfo GetAcquireWaitInfo() const;
        VkSemaphoreSubmitInfo GetPresentSignalInfo() const;

    private:
        void Destroy();

        VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
        VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);
        VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D extent);

    private:
        std::shared_ptr<Instance> m_Instance;
        std::shared_ptr<Device> m_Device;

        VkSwapchainKHR m_Swapchain { VK_NULL_HANDLE };
        VkFormat m_Format { VK_FORMAT_UNDEFINED };
        VkExtent2D m_Extent { 0, 0 };

        u32 m_ImageCount { 0 };
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;

        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_PresentSemaphores;

        u32 m_CurrentImageIndex { 0 };
        u32 m_CurrentSyncIndex { 0 };
    };

}