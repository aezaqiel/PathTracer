#include "Swapchain.hpp"

#include "Instance.hpp"
#include "Device.hpp"
#include "Image.hpp"

namespace RHI {

    Swapchain::Swapchain(const std::shared_ptr<Instance>& instance, const std::shared_ptr<Device>& device)
        : m_Instance(instance), m_Device(device)
    {
    }

    Swapchain::~Swapchain()
    {
        Destroy();
        vkDestroySwapchainKHR(m_Device->GetDevice(), m_Swapchain, nullptr);
    }

    void Swapchain::Create(u32 width, u32 height)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Device->GetPhysicalDevice(), m_Instance->GetSurface(), &capabilities);

        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->GetPhysicalDevice(), m_Instance->GetSurface(), &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->GetPhysicalDevice(), m_Instance->GetSurface(), &formatCount, formats.data());

        u32 modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->GetPhysicalDevice(), m_Instance->GetSurface(), &modeCount, nullptr);
        std::vector<VkPresentModeKHR> modes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->GetPhysicalDevice(), m_Instance->GetSurface(), &modeCount, modes.data());

        VkSurfaceFormatKHR format = ChooseSurfaceFormat(formats);
        VkPresentModeKHR mode = ChoosePresentMode(modes);
        VkExtent2D extent = ChooseExtent(capabilities, VkExtent2D { width, height });

        m_Format = format.format;
        m_Extent = extent;

        u32 imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        VkSwapchainKHR old = m_Swapchain;

        VkSwapchainCreateInfoKHR swapchainInfo {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = m_Instance->GetSurface(),
            .minImageCount = imageCount,
            .imageFormat = format.format,
            .imageColorSpace = format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = mode,
            .clipped = VK_TRUE,
            .oldSwapchain = old
        };

        VK_CHECK(vkCreateSwapchainKHR(m_Device->GetDevice(), &swapchainInfo, nullptr, &m_Swapchain));

        if (old != VK_NULL_HANDLE) {
            Destroy();
            vkDestroySwapchainKHR(m_Device->GetDevice(), old, nullptr);
        }

        vkGetSwapchainImagesKHR(m_Device->GetDevice(), m_Swapchain, &m_ImageCount, nullptr);

        m_Images.resize(m_ImageCount);
        std::vector<VkImage> images(m_ImageCount);

        vkGetSwapchainImagesKHR(m_Device->GetDevice(), m_Swapchain, &m_ImageCount, images.data());

        for (u32 i = 0; i < m_ImageCount; ++i) {
            m_Images[i] = std::make_shared<Image>(m_Device, images[i], Image::Spec {
                .extent = { m_Extent.width, m_Extent.height, 1 },
                .format = m_Format,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .memory = VMA_MEMORY_USAGE_UNKNOWN
            });
        }

        VkSemaphoreTypeCreateInfo semaphoreType {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
            .initialValue = 0
        };

        VkSemaphoreCreateInfo semaphoreInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &semaphoreType,
            .flags = 0
        };

        m_ImageAvailableSemaphores.resize(m_ImageCount);
        m_PresentSemaphores.resize(m_ImageCount);

        for (u32 i = 0; i < m_ImageCount; ++i) {
            VK_CHECK(vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]));
            VK_CHECK(vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_PresentSemaphores[i]));
        }
    }

    std::optional<VkResult> Swapchain::AcquireNextImage()
    {
        VkResult result = vkAcquireNextImageKHR(
            m_Device->GetDevice(),
            m_Swapchain,
            std::numeric_limits<u64>::max(),
            m_ImageAvailableSemaphores[m_CurrentSyncIndex],
            VK_NULL_HANDLE,
            &m_CurrentImageIndex
        );

        if (result == VK_SUCCESS) {
            return std::nullopt;
        }

        return result;
    }

    std::optional<VkResult> Swapchain::Present()
    {
        VkPresentInfoKHR presentInfo {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_PresentSemaphores[m_CurrentSyncIndex],
            .swapchainCount = 1,
            .pSwapchains = &m_Swapchain,
            .pImageIndices = &m_CurrentImageIndex,
            .pResults = nullptr
        };

        VkResult result = vkQueuePresentKHR(m_Device->GetQueue<QueueType::Graphics>(), &presentInfo);

        m_CurrentSyncIndex = (m_CurrentSyncIndex + 1) % m_ImageCount;

        if (result == VK_SUCCESS) {
            return std::nullopt;
        }

        return result;
    }

    VkSemaphoreSubmitInfo Swapchain::GetAcquireWaitInfo() const
    {
        return VkSemaphoreSubmitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = m_ImageAvailableSemaphores[m_CurrentSyncIndex],
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };
    }

    VkSemaphoreSubmitInfo Swapchain::GetPresentSignalInfo() const
    {
        return VkSemaphoreSubmitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = m_PresentSemaphores[m_CurrentSyncIndex],
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };
    }

    void Swapchain::Destroy()
    {
        for (u32 i = 0; i < m_ImageCount; ++i) {
            vkDestroySemaphore(m_Device->GetDevice(), m_PresentSemaphores[i], nullptr);
            vkDestroySemaphore(m_Device->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
        }

        m_PresentSemaphores.clear();
        m_ImageAvailableSemaphores.clear();

        m_Images.clear();
    }

    VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
    {
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        LOG_WARN("Using fallback surface format for swapchain");
        return formats[0];
    }

    VkPresentModeKHR Swapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
    {
        for (const auto& mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }

        LOG_WARN("Using fallback present mode for swapchain");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D extent)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
            return capabilities.currentExtent;
        }

        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }

}
