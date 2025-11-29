#include "Image.hpp"

#include "Device.hpp"

namespace RHI {

    Image::Image(const std::shared_ptr<Device>& device, const Spec& spec)
        : m_Device(device), m_Extent(spec.extent), m_Format(spec.format)
    {
        VkImageCreateInfo imageInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = m_Format,
            .extent = m_Extent,
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

        VmaAllocationCreateInfo allocationInfo;
        memset(&allocationInfo, 0, sizeof(VmaAllocationCreateInfo));
        allocationInfo.usage = spec.memory;

        VK_CHECK(vmaCreateImage(m_Device->GetAllocator(), &imageInfo, &allocationInfo, &m_Image, &m_Allocation, nullptr));

        CreateView();
    }

    Image::Image(const std::shared_ptr<Device>& device, VkImage image, const Spec& spec)
        : m_Device(device), m_Image(image), m_Extent(spec.extent), m_Format(spec.format)
    {
        m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
        CreateView();
    }

    Image::~Image()
    {
        vkDestroyImageView(m_Device->GetDevice(), m_View, nullptr);

        if (m_Allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(m_Device->GetAllocator(), m_Image, m_Allocation);
        }
    }

    void Image::TransitionLayout(VkCommandBuffer cmd, VkImageLayout layout,
        VkPipelineStageFlags2 srcStage,
        VkPipelineStageFlags2 dstStage,
        VkAccessFlags2 srcAccess,
        VkAccessFlags2 dstAccess,
        u32 srcQueue,
        u32 dstQueue
    )
    {
        if (m_Layout == layout && srcQueue == VK_QUEUE_FAMILY_IGNORED && dstQueue == VK_QUEUE_FAMILY_IGNORED) return;

        VkImageMemoryBarrier2 barrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = srcStage,
            .srcAccessMask = srcAccess,
            .dstStageMask = dstStage,
            .dstAccessMask = dstAccess,
            .oldLayout = m_Layout,
            .newLayout = layout,
            .srcQueueFamilyIndex = srcQueue,
            .dstQueueFamilyIndex = dstQueue,
            .image = m_Image,
            .subresourceRange = {
                .aspectMask = GetAspectFlags(),
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        if (m_Layout == VK_IMAGE_LAYOUT_UNDEFINED) {
            if (srcQueue == VK_QUEUE_FAMILY_IGNORED && dstQueue == VK_QUEUE_FAMILY_IGNORED) {
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_NONE;
            }
        }

        VkDependencyInfo dependency {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        };

        vkCmdPipelineBarrier2(cmd, &dependency);

        m_Layout = layout;
    }

    void Image::CreateView()
    {
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
                .aspectMask = GetAspectFlags(),
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VK_CHECK(vkCreateImageView(m_Device->GetDevice(), &viewInfo, nullptr, &m_View));
    }

    VkImageAspectFlags Image::GetAspectFlags() const
    {
        switch (m_Format) {
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D16_UNORM_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }

        LOG_WARN("Unexpected VkFormat for image");
        return VK_IMAGE_ASPECT_NONE;
    }

}
