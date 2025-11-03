#include "VulkanCommand.hpp"

namespace PathTracer {

    VulkanCommand::VulkanCommand(std::shared_ptr<VulkanDevice> device)
        : m_Device(device)
    {
        VkCommandPoolCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_Device->GetQueueFamilyIndex() 
        };

        VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &createInfo, nullptr, &m_CommandPool));

        VkFenceCreateInfo fenceInfo {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        VK_CHECK(vkCreateFence(m_Device->GetDevice(), &fenceInfo, nullptr, &m_Fence));
    }

    VulkanCommand::~VulkanCommand()
    {
        vkDestroyFence(m_Device->GetDevice(), m_Fence, nullptr);
        vkDestroyCommandPool(m_Device->GetDevice(), m_CommandPool, nullptr);
    }

    void VulkanCommand::SubmitOnce(const std::function<void(VkCommandBuffer)>& task)
    {
        VkCommandBufferAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VK_CHECK(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocateInfo, &m_CommandBuffer));

        VkCommandBufferBeginInfo beginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
        task(m_CommandBuffer);
        VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));

        VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &m_CommandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        VK_CHECK(vkQueueSubmit(m_Device->GetQueueFamily(), 1, &submitInfo, m_Fence));

        VK_CHECK(vkWaitForFences(m_Device->GetDevice(), 1, &m_Fence, VK_TRUE, std::numeric_limits<u64>::max()));
        VK_CHECK(vkResetFences(m_Device->GetDevice(), 1, &m_Fence));

        vkFreeCommandBuffers(m_Device->GetDevice(), m_CommandPool, 1, &m_CommandBuffer);
    }

}
