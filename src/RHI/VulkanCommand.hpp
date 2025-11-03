#pragma once

#include "VulkanDevice.hpp"

namespace PathTracer {

    class VulkanCommand
    {
    public:
        VulkanCommand(std::shared_ptr<VulkanDevice> device);
        ~VulkanCommand();

        void SubmitOnce(const std::function<void(VkCommandBuffer)>& task);

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        VkCommandPool m_CommandPool { VK_NULL_HANDLE };
        VkCommandBuffer m_CommandBuffer { VK_NULL_HANDLE };

        VkFence m_Fence { VK_NULL_HANDLE };
    };

}
