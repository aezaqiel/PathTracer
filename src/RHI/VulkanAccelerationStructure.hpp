#pragma once

#include <glm/glm.hpp>

#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanCommandManager.hpp"

namespace PathTracer {

    class VulkanAccelerationStructure
    {
    public:
        virtual ~VulkanAccelerationStructure();

        inline VkAccelerationStructureKHR GetAS() const { return m_AS; }
        inline std::shared_ptr<VulkanBuffer> GetBuffer() const { return m_Buffer; }
        inline VkDeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }

    protected:
        VulkanAccelerationStructure(std::shared_ptr<VulkanDevice> device);

        void Finalize(VkAccelerationStructureKHR handle, std::shared_ptr<VulkanBuffer> buffer);

    protected:
        std::shared_ptr<VulkanDevice> m_Device;

        VkAccelerationStructureKHR m_AS { VK_NULL_HANDLE };
        VkDeviceAddress m_DeviceAddress { 0 };
        std::shared_ptr<VulkanBuffer> m_Buffer;
    };

    class VulkanBLAS final : public VulkanAccelerationStructure
    {
    public:
        struct BLASGeometry
        {
            std::shared_ptr<VulkanBuffer> vertexBuffer;
            std::shared_ptr<VulkanBuffer> indexBuffer;
            u32 vertexCount;
            u32 indexCount;
            VkFormat vertexFormat;
            VkDeviceSize vertexStride;
            bool isOpaque { true };
        };

    public:
        VulkanBLAS(std::shared_ptr<VulkanDevice> device, const std::shared_ptr<VulkanCommandManager>& command, const std::vector<BLASGeometry>& geometries);
        virtual ~VulkanBLAS() = default;
    };

    class VulkanTLAS final : public VulkanAccelerationStructure
    {
    public:
        struct TLASInstance
        {
            std::shared_ptr<VulkanBLAS> blas;
            glm::mat4 transform { 1.0f };
            u32 instanceCustomIndex { 0 };
            u32 mask { 0xFF };
            u32 sbtOffset { 0 };
            VkGeometryInstanceFlagsKHR flags { VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR };
        };

    public:
        VulkanTLAS(std::shared_ptr<VulkanDevice> device, const std::shared_ptr<VulkanCommandManager>& command, const std::vector<TLASInstance>& instances);
        virtual ~VulkanTLAS() = default;
    };

}
