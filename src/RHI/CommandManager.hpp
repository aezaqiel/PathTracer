#pragma once

#include <memory>
#include <array>
#include <span>

#include "Device.hpp"

namespace RHI {

    template <QueueType TQueue>
    class CommandManager
    {
    public:
        CommandManager(const std::shared_ptr<Device>& device)
            : m_Device(device)
        {
            VkSemaphoreTypeCreateInfo semaphoreType {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .pNext = nullptr,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0
            };

            VkSemaphoreCreateInfo semaphoreInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = &semaphoreType,
                .flags = 0
            };

            VK_CHECK(vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_Timeline));

            for (u32 i = 0; i < Device::GetFrameInFlight(); ++i) {
                VkCommandPoolCreateInfo poolInfo {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = m_Device->GetQueueFamily<TQueue>()
                };

                VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_CommandPools[i]));

                VkCommandBufferAllocateInfo allocateInfo {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .pNext = nullptr,
                    .commandPool = m_CommandPools[i],
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1
                };

                VK_CHECK(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocateInfo, &m_CommandBuffers[i]));

                m_FrameTimeline[i] = 0;
            }
        }

        ~CommandManager()
        {
            Sync();

            for (u32 i = 0; i < Device::GetFrameInFlight(); ++i) {
                vkDestroyCommandPool(m_Device->GetDevice(), m_CommandPools[i], nullptr);
            }

            vkDestroySemaphore(m_Device->GetDevice(), m_Timeline, nullptr);
        }

        CommandManager(const CommandManager&) = delete;
        CommandManager& operator=(const CommandManager&) = delete;

        void Record(std::function<void(VkCommandBuffer)> func)
        {
            u32 index = m_LocalFrameIndex % Device::GetFrameInFlight();
            u64 timeline = m_FrameTimeline[index];

            Wait(timeline);

            VK_CHECK(vkResetCommandPool(m_Device->GetDevice(), m_CommandPools[index], 0));

            VkCommandBufferBeginInfo begin {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pInheritanceInfo = nullptr
            };

            VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[index], &begin));
            func(m_CommandBuffers[index]);
            VK_CHECK(vkEndCommandBuffer(m_CommandBuffers[index]));
        }

        u64 Submit(const std::span<VkSemaphoreSubmitInfo>& wait, const std::span<VkSemaphoreSubmitInfo>& signal)
        {
            u32 index = m_LocalFrameIndex % Device::GetFrameInFlight();
            u64 timeline = ++m_TimelineValue;

            std::vector<VkSemaphoreSubmitInfo> signals(signal.begin(), signal.end());
            signals.push_back(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = m_Timeline,
                .value = timeline,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .deviceIndex = 0
            });

            VkCommandBufferSubmitInfo cmd {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = m_CommandBuffers[index],
                .deviceMask = 0
            };

            VkSubmitInfo2 submit {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext = nullptr,
                .flags = 0,
                .waitSemaphoreInfoCount = static_cast<u32>(wait.size()),
                .pWaitSemaphoreInfos = wait.data(),
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &cmd,
                .signalSemaphoreInfoCount = static_cast<u32>(signals.size()),
                .pSignalSemaphoreInfos = signals.data()
            };

            VK_CHECK(vkQueueSubmit2(m_Device->GetQueue<TQueue>(), 1, &submit, VK_NULL_HANDLE));

            m_FrameTimeline[index] = timeline;
            m_LocalFrameIndex++;

            return timeline;
        }

        inline void Wait(u64 value)
        {
            VkSemaphoreWaitInfo wait {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .pNext = nullptr,
                .flags = 0,
                .semaphoreCount = 1,
                .pSemaphores = &m_Timeline,
                .pValues = &value
            };

            VK_CHECK(vkWaitSemaphores(m_Device->GetDevice(), &wait, std::numeric_limits<u64>::max()));
        }

        inline void Sync()
        {
            Wait(m_TimelineValue);
        }

        inline u64 GetCurrentValue() const { return m_TimelineValue; }

    private:
        std::shared_ptr<Device> m_Device;

        std::array<VkCommandPool, Device::GetFrameInFlight()> m_CommandPools;
        std::array<VkCommandBuffer, Device::GetFrameInFlight()> m_CommandBuffers;
        std::array<u64, Device::GetFrameInFlight()> m_FrameTimeline;

        u64 m_LocalFrameIndex { 0 };

        VkSemaphore m_Timeline { VK_NULL_HANDLE };
        u64 m_TimelineValue { 0 };
    };

}
