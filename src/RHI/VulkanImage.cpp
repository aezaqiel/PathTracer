#include "VulkanImage.hpp"

namespace PathTracer {

    VulkanImage::VulkanImage(std::shared_ptr<VulkanDevice> device, const Spec& spec)
        :   m_Device(device),
            m_Extent({spec.width, spec.height}),
            m_Format(spec.format),
            m_Layout(VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageCreateInfo imageInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = spec.format,
            .extent = {
                m_Extent.width,
                m_Extent.height,
                1
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = spec.usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = m_Layout
        };

        VK_CHECK(vkCreateImage(m_Device->GetDevice(), &imageInfo, nullptr, &m_Image));

        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(m_Device->GetDevice(), m_Image, &memoryRequirements);

        VkMemoryAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = FindMemoryType(m_Device->GetPhysicalDevice(), memoryRequirements.memoryTypeBits, spec.memoryProperties)
        };

        VK_CHECK(vkAllocateMemory(m_Device->GetDevice(), &allocateInfo, nullptr, &m_Memory));
        VK_CHECK(vkBindImageMemory(m_Device->GetDevice(), m_Image, m_Memory, 0));

        VkImageViewCreateInfo viewInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = m_Image,
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

        VK_CHECK(vkCreateImageView(m_Device->GetDevice(), &viewInfo, nullptr, &m_View));
    }

    VulkanImage::~VulkanImage()
    {
        vkDestroyImageView(m_Device->GetDevice(), m_View, nullptr);
        vkDestroyImage(m_Device->GetDevice(), m_Image, nullptr);
        vkFreeMemory(m_Device->GetDevice(), m_Memory, nullptr);
    }

}
