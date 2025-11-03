#pragma once

#include "VulkanDevice.hpp"

namespace PathTracer {

    class VulkanImage
    {
    public:
        struct Spec
        {
            u32 width;
            u32 height;
            VkFormat format;
            VkImageUsageFlags usage;
            VmaMemoryUsage memoryUsage;
        };

    public:
        VulkanImage(std::shared_ptr<VulkanDevice> device, const Spec& spec);
        ~VulkanImage();

        inline VkImage GetImage() const { return m_Image; }
        inline VkImageView GetView() const { return m_View; }

        inline VkExtent2D GetExtent() const { return m_Extent; }
        inline u32 GetWidth() const { return m_Extent.width; }
        inline u32 GetHeight() const { return m_Extent.height; }

        inline VkFormat GetFormat() const { return m_Format; }
        inline VkImageLayout GetLayout() const { return m_Layout; }

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        VkImage m_Image { VK_NULL_HANDLE };
        VmaAllocation m_Allocation { VK_NULL_HANDLE };

        VkImageView m_View { VK_NULL_HANDLE };

        VkExtent2D m_Extent;
        VkFormat m_Format;
        VkImageLayout m_Layout;
    };

}
