#pragma once

#include "Device.hpp"

namespace RHI {

    template <QueueType TQueue>
    class CommandContext
    {
    public:
        CommandContext(const std::shared_ptr<Device>& device)
            : m_Device(device)
        {
            for (u32 i = 0; i < Device::GetFrameInFlight(); ++i) {
                VkCommandPoolCreateInfo poolInfo {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = m_Device->GetQueueFamily<TQueue>()
                };

                VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_Pools[i]));

                VkCommandBufferAllocateInfo allocateInfo {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .pNext = nullptr,
                    .commandPool = m_Pools[i],
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1
                };

                VK_CHECK(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocateInfo, &m_Buffers[i]));
            }
        }

        ~CommandContext()
        {
            for (u32 i = 0; i < Device::GetFrameInFlight(); ++i) {
                vkDestroyCommandPool(m_Device->GetDevice(), m_Pools[i], nullptr);
            }
        }

        VkCommandBuffer Record(std::function<void(VkCommandBuffer)> func)
        {
            VK_CHECK(vkResetCommandPool(m_Device->GetDevice(), m_Pools[m_Device->GetCurrentFrameIndex()], 0));

            VkCommandBufferBeginInfo beginInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr
            };

            VkCommandBuffer cmd = m_Buffers[m_Device->GetCurrentFrameIndex()];

            VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
            func(cmd);
            VK_CHECK(vkEndCommandBuffer(cmd));

            return cmd;
        }

    private:
        std::shared_ptr<Device> m_Device;

        PerFrame<VkCommandPool> m_Pools;
        PerFrame<VkCommandBuffer> m_Buffers;
    };

}
