#pragma once

#include "Core/Window.hpp"

#include "RHI/VulkanContext.hpp"
#include "RHI/VulkanDevice.hpp"
#include "RHI/VulkanSwapchain.hpp"
#include "RHI/VulkanCommand.hpp"
#include "RHI/VulkanImage.hpp"
#include "RHI/VulkanBuffer.hpp"
#include "RHI/VulkanAccelerationStructure.hpp"
#include "RHI/VulkanDescriptorSetLayout.hpp"
#include "RHI/VulkanPipelineLayout.hpp"
#include "RHI/VulkanPipeline.hpp"
#include "RHI/VulkanDescriptorPool.hpp"
#include "RHI/VulkanShaderBindingTable.hpp"

namespace PathTracer {

    class Renderer
    {
    public:
        struct RenderPacket
        {
        };

    public:
        Renderer(std::shared_ptr<Window> window);
        ~Renderer();

        void RequestResize(u32 width, u32 height);
        void Submit(const RenderPacket& packet);

    private:
        struct ResizeRequest
        {
            u32 width { 0 };
            u32 height { 0 };
        };

    private:
        void RenderLoop();

        void Draw(const RenderPacket& packet);
        void HandleResize(const ResizeRequest& request);

    private:
        std::shared_ptr<Window> m_Window;

        u32 m_Width { 0 };
        u32 m_Height { 0 };

        std::thread m_RenderThread;
        std::atomic<bool> m_Running { true };

        std::mutex m_RenderMutex;
        std::condition_variable m_RenderCondition;

        std::mutex m_SubmitMutex;
        RenderPacket m_RenderPacket;
        std::atomic<bool> m_PacketSubmitted { false };

        std::mutex m_ResizeMutex;
        ResizeRequest m_ResizeRequest;
        std::atomic<bool> m_ResizeRequested { false };

        std::shared_ptr<VulkanContext> m_Context;
        std::shared_ptr<VulkanDevice> m_Device;
        std::shared_ptr<VulkanSwapchain> m_Swapchain;
        std::shared_ptr<VulkanCommand> m_Command;

        std::shared_ptr<VulkanImage> m_StorageImage;
        std::shared_ptr<VulkanBuffer> m_StagingBuffer;

        std::shared_ptr<VulkanBuffer> m_VertexBuffer;
        std::shared_ptr<VulkanBuffer> m_IndexBuffer;

        std::shared_ptr<VulkanBLAS> m_BLAS;
        std::shared_ptr<VulkanTLAS> m_TLAS;

        std::shared_ptr<VulkanDescriptorPool> m_DescriptorPool;
        std::shared_ptr<VulkanDescriptorSetLayout> m_DescriptorSetLayout;
        VkDescriptorSet m_DescriptorSet { VK_NULL_HANDLE };

        std::shared_ptr<VulkanPipelineLayout> m_PipelineLayout;
        std::shared_ptr<VulkanRayTracingPipeline> m_Pipeline;

        std::shared_ptr<VulkanShaderBindingTable> m_SBT;
    };

}
