#include "VulkanShaderBindingTable.hpp"

namespace PathTracer {

    VulkanShaderBindingTable::Builder::Builder(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanRayTracingPipeline> pipeline)
        : m_Device(device), m_Pipeline(pipeline)
    {
    }

    VulkanShaderBindingTable::Builder& VulkanShaderBindingTable::Builder::AddRayGenGroup(const void* pData, u32 size)
    {
        m_RayGenCount++;

        const auto& rtProps = m_Device->GetRtProps();
        const u32 handleSize = rtProps.shaderGroupHandleSize;
        const u32 handleAlignment = rtProps.shaderGroupBaseAlignment;
        const u32 entryStride = AlignedSize(handleSize, handleAlignment);

        const u32 shaderRecordDataSize = entryStride - handleSize;
        if (size > shaderRecordDataSize) {
            std::println("Raygen group size > shader record data size");
        }

        std::vector<u8> record(shaderRecordDataSize, 0);
        if (pData && size > 0) {
            memcpy(record.data(), pData, std::min(size, shaderRecordDataSize));
        }

        m_ShaderRecordData.insert(m_ShaderRecordData.end(), record.begin(), record.end());

        return *this;
    }

    VulkanShaderBindingTable::Builder& VulkanShaderBindingTable::Builder::AddMissGroup(const void* pData, u32 size)
    {
        m_MissCount++;

        const auto& rtProps = m_Device->GetRtProps();
        const u32 handleSize = rtProps.shaderGroupHandleSize;
        const u32 handleAlignment = rtProps.shaderGroupBaseAlignment;
        const u32 entryStride = AlignedSize(handleSize, handleAlignment);

        const u32 shaderRecordDataSize = entryStride - handleSize;
        if (size > shaderRecordDataSize) {
            std::println("Miss group size > shader record data size");
        }

        std::vector<u8> record(shaderRecordDataSize, 0);
        if (pData && size > 0) {
            memcpy(record.data(), pData, std::min(size, shaderRecordDataSize));
        }

        m_ShaderRecordData.insert(m_ShaderRecordData.end(), record.begin(), record.end());

        return *this;
    }

    VulkanShaderBindingTable::Builder& VulkanShaderBindingTable::Builder::AddHitGroup(const void* pData, u32 size)
    {
        m_HitCount++;

        const auto& rtProps = m_Device->GetRtProps();
        const u32 handleSize = rtProps.shaderGroupHandleSize;
        const u32 handleAlignment = rtProps.shaderGroupBaseAlignment;
        const u32 entryStride = AlignedSize(handleSize, handleAlignment);

        const u32 shaderRecordDataSize = entryStride - handleSize;
        if (size > shaderRecordDataSize) {
            std::println("Hit group size > shader record data size");
        }

        std::vector<u8> record(shaderRecordDataSize, 0);
        if (pData && size > 0) {
            memcpy(record.data(), pData, std::min(size, shaderRecordDataSize));
        }

        m_ShaderRecordData.insert(m_ShaderRecordData.end(), record.begin(), record.end());

        return *this;
    }

    std::shared_ptr<VulkanShaderBindingTable> VulkanShaderBindingTable::Builder::Build()
    {
        return std::make_shared<VulkanShaderBindingTable>(
            m_Device, m_Pipeline, m_RayGenCount, m_MissCount, m_HitCount, std::span<u8>(m_ShaderRecordData)
        );
    }

    VulkanShaderBindingTable::VulkanShaderBindingTable(std::shared_ptr<VulkanDevice> device, const std::shared_ptr<VulkanRayTracingPipeline>& pipeline, u32 rayGenCount, u32 missCount, u32 hitCount, std::span<u8> shaderRecordData)
        : m_Device(device)
    {
        const auto& rtProps = m_Device->GetRtProps();
        const u32 handleSize = rtProps.shaderGroupHandleSize;
        const u32 handleAlignment = rtProps.shaderGroupBaseAlignment;
        const u32 entryStride = AlignedSize(handleSize, handleAlignment);

        const u32 shaderRecordDataSize = entryStride - handleSize;

        const u32 rayGenSize = rayGenCount * entryStride;
        const u32 missSize = missCount * entryStride;
        const u32 hitSize = hitCount * entryStride;

        const u32 totalGroupCount = rayGenCount + missCount + hitCount;
        const u32 totalDataSize = totalGroupCount * handleSize;
        const u32 sbtSize = totalGroupCount * entryStride;

        std::vector<u8> shaderHandles(totalDataSize);
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
            m_Device->GetDevice(),
            pipeline->GetPipeline(),
            0,
            totalGroupCount,
            totalDataSize,
            shaderHandles.data()
        ));

        m_Buffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sbtSize,
            .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        });

        u8* sbtData = reinterpret_cast<u8*>(m_Buffer->Map());
        u32 dataOffset = 0;
        u32 sbtOffset = 0;

        for (u32 i = 0; i < rayGenCount; ++i) {
            memcpy(sbtData + sbtOffset, shaderHandles.data() + (i * handleSize), handleSize);

            if (shaderRecordDataSize > 0) {
                memcpy(sbtData + sbtOffset + handleSize, shaderRecordData.data() + dataOffset, shaderRecordDataSize);
                dataOffset += shaderRecordDataSize;
            }

            sbtOffset += entryStride;
        }

        for (u32 i = 0; i < missCount; ++i) {
            memcpy(sbtData + sbtOffset, shaderHandles.data() + ((i + rayGenCount) * handleSize), handleSize);

            if (shaderRecordDataSize > 0) {
                memcpy(sbtData + sbtOffset + handleSize, shaderRecordData.data() + dataOffset, shaderRecordDataSize);
                dataOffset += shaderRecordDataSize;
            }

            sbtOffset += entryStride;
        }

        for (u32 i = 0; i < hitCount; ++i) {
            memcpy(sbtData + sbtOffset, shaderHandles.data() + ((i + rayGenCount + missCount) * handleSize), handleSize);

            if (shaderRecordDataSize > 0) {
                memcpy(sbtData + sbtOffset + handleSize, shaderRecordData.data() + dataOffset, shaderRecordDataSize);
                dataOffset += shaderRecordDataSize;
            }

            sbtOffset += entryStride;
        }

        m_Buffer->Unmap();

        VkDeviceAddress sbtAddress = m_Buffer->GetDeviceAddress();

        m_RayGenRegion = VkStridedDeviceAddressRegionKHR {
            .deviceAddress = sbtAddress,
            .stride = entryStride,
            .size = rayGenSize
        };

        m_MissRegion = VkStridedDeviceAddressRegionKHR {
            .deviceAddress = sbtAddress + rayGenSize,
            .stride = entryStride,
            .size = missSize
        };

        m_HitRegion = VkStridedDeviceAddressRegionKHR {
            .deviceAddress = sbtAddress + rayGenSize + missSize,
            .stride = entryStride,
            .size = hitSize
        };

        m_CallbableRegion = VkStridedDeviceAddressRegionKHR {
            .deviceAddress = 0,
            .stride = 0,
            .size = 0
        };
    }

    u32 VulkanShaderBindingTable::AlignedSize(u32 size, u32 alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }

}
