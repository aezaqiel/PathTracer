#include "VulkanSwapchain.hpp"

namespace PathTracer {

    VulkanSwapchain::VulkanSwapchain(std::shared_ptr<VulkanContext> context, std::shared_ptr<VulkanDevice> device)
        : m_Context(context), m_Device(device)
    {
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        Destroy();
        vkDestroySwapchainKHR(m_Device->GetDevice(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }

    void VulkanSwapchain::Recreate(u32 width, u32 height)
    {
        Create(width, height);
    }

    VkResult VulkanSwapchain::AcquireNextImage(VkSemaphore signalSemaphore, u32& imageIndex)
    {
        return vkAcquireNextImageKHR(
            m_Device->GetDevice(),
            m_Swapchain,
            std::numeric_limits<u64>::max(),
            signalSemaphore,
            VK_NULL_HANDLE,
            &imageIndex
        );
    }

    VkResult VulkanSwapchain::Present(VkQueue presentQueue, std::span<VkSemaphore> waitSemaphores, u32 imageIndex, VkFence signalFence)
    {
        VkSwapchainPresentFenceInfoKHR presentFence {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
            .pNext = nullptr,
            .swapchainCount = 1,
            .pFences = &signalFence
        };

        VkPresentInfoKHR presentInfo {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = &presentFence,
            .waitSemaphoreCount = static_cast<u32>(waitSemaphores.size()),
            .pWaitSemaphores = waitSemaphores.data(),
            .swapchainCount = 1,
            .pSwapchains = &m_Swapchain,
            .pImageIndices = &imageIndex,
            .pResults = nullptr
        };

        return vkQueuePresentKHR(presentQueue, &presentInfo);
    }

    void VulkanSwapchain::Create(u32 width, u32 height)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Device->GetPhysicalDevice(), m_Context->GetSurface(), &capabilities);

        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->GetPhysicalDevice(), m_Context->GetSurface(), &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->GetPhysicalDevice(), m_Context->GetSurface(), &formatCount, formats.data());

        u32 presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->GetPhysicalDevice(), m_Context->GetSurface(), &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->GetPhysicalDevice(), m_Context->GetSurface(), &presentModeCount, presentModes.data());

        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
        VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
        VkExtent2D extent = ChooseExtent(capabilities, width, height);

        m_Format = surfaceFormat.format;
        m_Extent = extent;

        u32 imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        VkSwapchainKHR old = m_Swapchain;

        VkSwapchainCreateInfoKHR createInfo {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = m_Context->GetSurface(),
            .minImageCount = imageCount,
            .imageFormat = m_Format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = m_Extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = old
        };

        VK_CHECK(vkCreateSwapchainKHR(m_Device->GetDevice(), &createInfo, nullptr, &m_Swapchain));

        if (old != VK_NULL_HANDLE) {
            Destroy();
            vkDestroySwapchainKHR(m_Device->GetDevice(), old, nullptr);
        }

        vkGetSwapchainImagesKHR(m_Device->GetDevice(), m_Swapchain, &m_ImageCount, nullptr);
        m_Images.resize(m_ImageCount);
        m_ImageViews.resize(m_ImageCount);
        vkGetSwapchainImagesKHR(m_Device->GetDevice(), m_Swapchain, &m_ImageCount, m_Images.data());

        for (usize i = 0; i < m_ImageCount; ++i) {
            VkImageViewCreateInfo viewInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = m_Images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = m_Format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            VK_CHECK(vkCreateImageView(m_Device->GetDevice(), &viewInfo, nullptr, &m_ImageViews[i]));
        }

        LOG_INFO("Vulkan swapchain created ({}, {}) with {} images", m_Extent.width, m_Extent.height, m_ImageCount);
    }

    void VulkanSwapchain::Destroy()
    {
        for (auto& view : m_ImageViews) {
            vkDestroyImageView(m_Device->GetDevice(), view, nullptr);
        }

        m_ImageViews.clear();
        m_Images.clear();
    }

    VkSurfaceFormatKHR VulkanSwapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
    {
        for (const auto& format : surfaceFormats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        LOG_WARN("Using fallback surface format for swapchain");
        return surfaceFormats[0];
    }

    VkPresentModeKHR VulkanSwapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes)
    {
        for (const auto& mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }

        LOG_WARN("Using fallback present mode for swapchain");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanSwapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
            return capabilities.currentExtent;
        }

        VkExtent2D extent = {
            .width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            .height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };

        return extent;
    }

}
