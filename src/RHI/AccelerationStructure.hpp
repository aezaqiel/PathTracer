#pragma once

#include <glm/glm.hpp>

#include "VkTypes.hpp"
#include "CommandContext.hpp"

namespace RHI {

    class Device;
    class Buffer;

    class AccelerationStructure
    {
    public:
        virtual ~AccelerationStructure();

        VkAccelerationStructureKHR GetAS() const { return m_AS; }
        Buffer* GetBuffer() const { return m_Buffer.get(); }
        VkDeviceAddress GetAddress() const { return m_Address; }

    protected:
        AccelerationStructure(const std::shared_ptr<Device>& device);

    protected:
        std::shared_ptr<Device> m_Device;

        VkAccelerationStructureKHR m_AS { VK_NULL_HANDLE };
        std::unique_ptr<Buffer> m_Buffer;
        VkDeviceAddress m_Address { 0 };
    };

    class BLAS final : public AccelerationStructure
    {
    public:
        struct Geometry
        {
            struct
            {
                Buffer* buffer { nullptr };
                u32 count { 0 };
                VkDeviceSize stride { 0 };
                VkDeviceSize offset { 0 };
                VkFormat format { VK_FORMAT_UNDEFINED };
            } vertices;

            struct
            {
                Buffer* buffer { nullptr };
                u32 count { 0 };
                VkDeviceSize offset { 0 };
            } indices;

            bool isOpaque { true };
        };

    public:
        BLAS(const std::shared_ptr<Device>& device, CommandContext<QueueType::Compute>& queue, const std::span<Geometry>& geometries);
        virtual ~BLAS() = default;
    };

    class TLAS final : public AccelerationStructure
    {
    public:
        struct Instance
        {
            BLAS* blas;
            glm::mat4 transform { 1.0f };
            u32 instanceCustomIndex { 0 };
            u32 mask { 0xFF };
            u32 sbtOffset { 0 };
            VkGeometryInstanceFlagsKHR flags { VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR };
        };

    public:
        TLAS(const std::shared_ptr<Device>& device, CommandContext<QueueType::Compute>& queue, const std::span<Instance>& instances);
        virtual ~TLAS() = default;
    };

}
