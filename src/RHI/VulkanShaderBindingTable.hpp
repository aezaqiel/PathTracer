#pragma once

#include "VulkanDevice.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanBuffer.hpp"

namespace PathTracer {

    class VulkanShaderBindingTable
    {
    public:
        class Builder
        {
        public:
            Builder(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanRayTracingPipeline> pipeline);
            ~Builder() = default;

            Builder& AddRayGenGroup(const void* pData = nullptr, u32 size = 0);
            Builder& AddMissGroup(const void* pData = nullptr, u32 size = 0);
            Builder& AddHitGroup(const void* pData = nullptr, u32 size = 0);

            std::shared_ptr<VulkanShaderBindingTable> Build();

        private:
            std::shared_ptr<VulkanDevice> m_Device;
            std::shared_ptr<VulkanRayTracingPipeline> m_Pipeline;

            u32 m_RayGenCount { 0 };
            u32 m_MissCount { 0 };
            u32 m_HitCount { 0 };

            std::vector<u8> m_ShaderRecordData;
        };

    public:
        VulkanShaderBindingTable(std::shared_ptr<VulkanDevice> device, const std::shared_ptr<VulkanRayTracingPipeline>& pipeline, u32 rayGenCount, u32 missCount, u32 hitCount, std::span<u8> shaderRecordData);
        ~VulkanShaderBindingTable() = default;

        inline VkStridedDeviceAddressRegionKHR GetRayGenRegion() const { return m_RayGenRegion;};
        inline VkStridedDeviceAddressRegionKHR GetMissRegion() const { return m_MissRegion; };
        inline VkStridedDeviceAddressRegionKHR GetHitRegion() const { return m_HitRegion; };
        inline VkStridedDeviceAddressRegionKHR GetCallbableRegion() const { return m_CallbableRegion; };

    private:
        static u32 AlignedSize(u32 size, u32 alignment);

    private:
        std::shared_ptr<VulkanDevice> m_Device;

        std::shared_ptr<VulkanBuffer> m_Buffer;
        VkStridedDeviceAddressRegionKHR m_RayGenRegion {};
        VkStridedDeviceAddressRegionKHR m_MissRegion {};
        VkStridedDeviceAddressRegionKHR m_HitRegion {};
        VkStridedDeviceAddressRegionKHR m_CallbableRegion {};
    };

}
