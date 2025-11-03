#pragma once

#include "RHI/VulkanContext.hpp"
#include "RHI/VulkanDevice.hpp"
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

    class Application
    {
    public:
        Application();
        ~Application() = default;

        void Run();
    
    private:
        std::shared_ptr<VulkanContext> m_Context;
        std::shared_ptr<VulkanDevice> m_Device;
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
