#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Device;

    class Image
    {
    public:
        struct Spec
        {
            VkExtent3D extent;
            VkFormat format;
            VkImageUsageFlags usage;
            VmaMemoryUsage memory;
        };

    public:
        Image(const Device& device, const Spec& spec);
        ~Image();

        inline VkImage GetImage() const { return m_Image; }
        inline VkImageView GetView() const { return m_View; }

        inline VkExtent3D GetExtent() const { return m_Extent; }
        inline VkFormat GetFormat() const { return m_Format; }
        inline VkImageLayout GetLayout() const { return m_Layout; }

    private:
        const Device& m_Device;

        VkImage m_Image { VK_NULL_HANDLE };
        VmaAllocation m_Allocation { VK_NULL_HANDLE };

        VkImageView m_View { VK_NULL_HANDLE };

        VkExtent3D m_Extent { 0, 0, 0 };
        VkFormat m_Format { VK_FORMAT_UNDEFINED };
        VkImageLayout m_Layout { VK_IMAGE_LAYOUT_UNDEFINED };
    };

}
