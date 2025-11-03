#include "VulkanCommandManager.hpp"

namespace PathTracer {

    VulkanCommandManager::VulkanCommandManager(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanSwapchain> swapchain)
        : m_Device(device), m_Swapchain(swapchain)
    {
        CreateFrameData();
        CreateSubmitOnceData();
    }

    VulkanCommandManager::~VulkanCommandManager()
    {
        WaitIdle();

        DestroySubmitOnceData();
        DestroyFrameData();
    }

    void VulkanCommandManager::SubmitOnce(QueueType queue, std::function<void(VkCommandBuffer)>&& func)
    {
        std::lock_guard lock(m_SubmitOnceMutex);

        VkQueue q;

        switch (queue) {
            case QueueType::Graphics: {
                q = m_Device->GetGraphicsQueue();
            } break;
            case QueueType::Transfer: {
                q = m_Device->GetTransferQueue();
            } break;
            case QueueType::Compute: {
                q = m_Device->GetComputeQueue();
            } break;
        }

        VkCommandBufferAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_SubmitOncePools[queue],
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer cmd;
        VK_CHECK(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocateInfo, &cmd));

        VkCommandBufferBeginInfo beginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
        func(cmd);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        VK_CHECK(vkResetFences(m_Device->GetDevice(), 1, &m_SubmitOnceFence));
        VK_CHECK(vkQueueSubmit(q, 1, &submitInfo, m_SubmitOnceFence));
        VK_CHECK(vkWaitForFences(m_Device->GetDevice(), 1, &m_SubmitOnceFence, VK_TRUE, std::numeric_limits<u64>::max()));

        vkFreeCommandBuffers(m_Device->GetDevice(), m_SubmitOncePools[queue], 1, &cmd);
    }

    std::optional<FrameContext> VulkanCommandManager::BeginFrame()
    {
        VK_CHECK(vkWaitForFences(m_Device->GetDevice(), 1, &m_Frames[m_CurrentFrame].inFlightFence, VK_TRUE, std::numeric_limits<u64>::max()));

        VkResult result = m_Swapchain->AcquireNextImage(m_Frames[m_CurrentFrame].imageAvailableSemaphore, m_CurrentImage);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return std::nullopt;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            LOG_FATAL("Failed to acquire swapchain image");
            std::abort();
        }

        VK_CHECK(vkResetFences(m_Device->GetDevice(), 1, &m_Frames[m_CurrentFrame].inFlightFence));

        VK_CHECK(vkResetCommandPool(m_Device->GetDevice(), m_Frames[m_CurrentFrame].graphicsPool, 0));
        if (m_Frames[m_CurrentFrame].transferPool != m_Frames[m_CurrentFrame].graphicsPool) {
            VK_CHECK(vkResetCommandPool(m_Device->GetDevice(), m_Frames[m_CurrentFrame].transferPool, 0));
        }

        if (m_Frames[m_CurrentFrame].computePool != m_Frames[m_CurrentFrame].graphicsPool && m_Frames[m_CurrentFrame].computePool != m_Frames[m_CurrentFrame].transferPool) {
            VK_CHECK(vkResetCommandPool(m_Device->GetDevice(), m_Frames[m_CurrentFrame].computePool, 0));
        }

        m_FrameInProgress = true;

        return FrameContext {
            .graphicsBuffer = m_Frames[m_CurrentFrame].graphicsBuffer,
            .transferBuffer = m_Frames[m_CurrentFrame].transferBuffer,
            .computeBuffer = m_Frames[m_CurrentFrame].computeBuffer,
            .imageIndex = m_CurrentImage,
            .frameIndex = m_CurrentFrame
        };
    }

    void VulkanCommandManager::EndFrame()
    {
        VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 0,
            .pCommandBuffers = nullptr,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_Frames[m_CurrentFrame].transferBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_Frames[m_CurrentFrame].transferFinishedSemaphore;
        submitInfo.waitSemaphoreCount = 0;
        VK_CHECK(vkQueueSubmit(m_Device->GetTransferQueue(), 1, &submitInfo, VK_NULL_HANDLE));

        VkPipelineStageFlags computeWaitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        submitInfo.pCommandBuffers = &m_Frames[m_CurrentFrame].computeBuffer;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_Frames[m_CurrentFrame].transferFinishedSemaphore;
        submitInfo.pWaitDstStageMask = &computeWaitStage;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_Frames[m_CurrentFrame].computeFinishedSemaphore;
        VK_CHECK(vkQueueSubmit(m_Device->GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));

        VkSemaphore graphicsWaitSemaphores[] = {
            m_Frames[m_CurrentFrame].imageAvailableSemaphore,
            // m_Frames[m_CurrentFrame].transferFinishedSemaphore,
            m_Frames[m_CurrentFrame].computeFinishedSemaphore
        };

        VkPipelineStageFlags graphicsWaitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT
            // VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            // VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
        };

        submitInfo.pCommandBuffers = &m_Frames[m_CurrentFrame].graphicsBuffer;
        submitInfo.waitSemaphoreCount = static_cast<u32>(sizeof(graphicsWaitSemaphores) / sizeof(graphicsWaitSemaphores[0]));
        submitInfo.pWaitSemaphores = graphicsWaitSemaphores;
        submitInfo.pWaitDstStageMask = graphicsWaitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_Frames[m_CurrentFrame].renderFinishedSemaphore;
        VK_CHECK(vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));

        VkSemaphore presentWaitSemaphore[] = { m_Frames[m_CurrentFrame].renderFinishedSemaphore };
        VK_CHECK(m_Swapchain->Present(
            m_Device->GetPresentQueue(),
            presentWaitSemaphore,
            m_CurrentImage,
            m_Frames[m_CurrentFrame].inFlightFence
        ));

        m_FrameInProgress = false;
        m_CurrentFrame = (m_CurrentFrame + 1) % s_FrameInFlight;
    }

    void VulkanCommandManager::CreateFrameData()
    {
        VkSemaphoreCreateInfo semaphoreInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };

        VkFenceCreateInfo fenceInfo {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        VkCommandPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };

        VkCommandBufferAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = VK_NULL_HANDLE,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        for (u32 i = 0; i < s_FrameInFlight; ++i) {
            VK_CHECK(vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_Frames[i].imageAvailableSemaphore));
            VK_CHECK(vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_Frames[i].renderFinishedSemaphore));
            VK_CHECK(vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_Frames[i].transferFinishedSemaphore));
            VK_CHECK(vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_Frames[i].computeFinishedSemaphore));
            VK_CHECK(vkCreateFence(m_Device->GetDevice(), &fenceInfo, nullptr, &m_Frames[i].inFlightFence));

            poolInfo.queueFamilyIndex = m_Device->GetGraphicsQueueIndex();
            VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_Frames[i].graphicsPool));

            if (m_Device->GetTransferQueueIndex() == m_Device->GetGraphicsQueueIndex()) {
                m_Frames[i].transferPool = m_Frames[i].graphicsPool;
            } else {
                poolInfo.queueFamilyIndex = m_Device->GetTransferQueueIndex();
                VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_Frames[i].transferPool));
            }

            if (m_Device->GetComputeQueueIndex() == m_Device->GetGraphicsQueueIndex()) {
                m_Frames[i].computePool = m_Frames[i].graphicsPool;
            } else if (m_Device->GetComputeQueueIndex() == m_Device->GetTransferQueueIndex()) {
                m_Frames[i].computePool = m_Frames[i].transferPool;
            } else {
                poolInfo.queueFamilyIndex = m_Device->GetComputeQueueIndex();
                VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_Frames[i].computePool));
            }

            allocateInfo.commandPool = m_Frames[i].graphicsPool;
            VK_CHECK(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocateInfo, &m_Frames[i].graphicsBuffer));

            allocateInfo.commandPool = m_Frames[i].transferPool;
            VK_CHECK(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocateInfo, &m_Frames[i].transferBuffer));

            allocateInfo.commandPool = m_Frames[i].computePool;
            VK_CHECK(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocateInfo, &m_Frames[i].computeBuffer));
        }
    }

    void VulkanCommandManager::DestroyFrameData()
    {
        for (auto& frame : m_Frames) {
            vkDestroyFence(m_Device->GetDevice(), frame.inFlightFence, nullptr);
            vkDestroySemaphore(m_Device->GetDevice(), frame.computeFinishedSemaphore, nullptr);
            vkDestroySemaphore(m_Device->GetDevice(), frame.transferFinishedSemaphore, nullptr);
            vkDestroySemaphore(m_Device->GetDevice(), frame.renderFinishedSemaphore, nullptr);
            vkDestroySemaphore(m_Device->GetDevice(), frame.imageAvailableSemaphore, nullptr);

            vkDestroyCommandPool(m_Device->GetDevice(), frame.graphicsPool, nullptr);

            if (frame.transferPool != frame.graphicsPool) {
                vkDestroyCommandPool(m_Device->GetDevice(), frame.transferPool, nullptr);
            }

            if (frame.computePool != frame.graphicsPool && frame.computePool != frame.transferPool) {
                vkDestroyCommandPool(m_Device->GetDevice(), frame.computePool, nullptr);
            }
        }
    }

    void VulkanCommandManager::CreateSubmitOnceData()
    {
        VkFenceCreateInfo fenceInfo {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        VK_CHECK(vkCreateFence(m_Device->GetDevice(), &fenceInfo, nullptr, &m_SubmitOnceFence));

        VkCommandPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
        };

        poolInfo.queueFamilyIndex = m_Device->GetGraphicsQueueIndex();
        VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_SubmitOncePools[QueueType::Graphics]));

        if (m_Device->GetTransferQueueIndex() == m_Device->GetGraphicsQueueIndex()) {
            m_SubmitOncePools[QueueType::Transfer] = m_SubmitOncePools[QueueType::Graphics];
        } else {
            poolInfo.queueFamilyIndex = m_Device->GetTransferQueueIndex();
            VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_SubmitOncePools[QueueType::Transfer]));
        }

        if (m_Device->GetComputeQueueIndex() == m_Device->GetGraphicsQueueIndex()) {
            m_SubmitOncePools[QueueType::Compute]= m_SubmitOncePools[QueueType::Graphics];
        } else if (m_Device->GetComputeQueueIndex() == m_Device->GetTransferQueueIndex()) {
            m_SubmitOncePools[QueueType::Compute]= m_SubmitOncePools[QueueType::Transfer];
        } else {
            poolInfo.queueFamilyIndex = m_Device->GetComputeQueueIndex();
            VK_CHECK(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_SubmitOncePools[QueueType::Compute]));
        }
    }

    void VulkanCommandManager::DestroySubmitOnceData()
    {
        vkDestroyFence(m_Device->GetDevice(), m_SubmitOnceFence, nullptr);

        vkDestroyCommandPool(m_Device->GetDevice(), m_SubmitOncePools[QueueType::Graphics], nullptr);

        if (m_SubmitOncePools[QueueType::Transfer] != m_SubmitOncePools[QueueType::Graphics]) {
            vkDestroyCommandPool(m_Device->GetDevice(), m_SubmitOncePools[QueueType::Transfer], nullptr);
        }

        if (m_SubmitOncePools[QueueType::Compute] != m_SubmitOncePools[QueueType::Graphics] && m_SubmitOncePools[QueueType::Compute] != m_SubmitOncePools[QueueType::Transfer]) {
            vkDestroyCommandPool(m_Device->GetDevice(), m_SubmitOncePools[QueueType::Compute], nullptr);
        }
    }

}
