#include "VulkanAccelerationStructure.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace PathTracer {

    namespace Utils {

        static VkTransformMatrixKHR ToVkTransformMatrix(const glm::mat4& mat)
        {
            const glm::mat4 transp = glm::transpose(mat);

            VkTransformMatrixKHR vkMat;
            memcpy(&vkMat, glm::value_ptr(transp), sizeof(VkTransformMatrixKHR));

            return vkMat;
        }
    
    }

    VulkanAccelerationStructure::VulkanAccelerationStructure(std::shared_ptr<VulkanDevice> device)
        : m_Device(device)
    {
    }

    VulkanAccelerationStructure::~VulkanAccelerationStructure()
    {
        if (m_AS != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR(m_Device->GetDevice(), m_AS, nullptr);
        }
    }

    VulkanBLAS::VulkanBLAS(std::shared_ptr<VulkanDevice> device, const std::shared_ptr<VulkanCommandManager>& command, const std::vector<BLASGeometry>& geometries)
        : VulkanAccelerationStructure(device)
    {
        std::vector<VkAccelerationStructureGeometryKHR> vkGeometries;
        vkGeometries.reserve(geometries.size());

        std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
        buildRanges.reserve(geometries.size());

        for (const auto& geo : geometries) {
            VkAccelerationStructureGeometryTrianglesDataKHR trianglesData {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .pNext = nullptr,
                .vertexFormat = geo.vertexFormat,
                .vertexData = {
                    .deviceAddress = geo.vertexBuffer->GetDeviceAddress()
                },
                .vertexStride = geo.vertexStride,
                .maxVertex = geo.vertexCount - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = {
                    .deviceAddress = geo.indexBuffer->GetDeviceAddress()
                },
                .transformData = { .deviceAddress = 0 }
            };

            vkGeometries.push_back(VkAccelerationStructureGeometryKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .pNext = nullptr,
                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                .geometry = { .triangles = trianglesData },
                .flags = geo.isOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0u
            });

            buildRanges.push_back(VkAccelerationStructureBuildRangeInfoKHR {
                .primitiveCount = geo.indexCount / 3,
                .primitiveOffset = 0,
                .firstVertex = 0,
                .transformOffset = 0
            });
        }

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .pNext = nullptr,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .srcAccelerationStructure = VK_NULL_HANDLE,
            .dstAccelerationStructure = VK_NULL_HANDLE,
            .geometryCount = static_cast<u32>(vkGeometries.size()),
            .pGeometries = vkGeometries.data(),
            .ppGeometries = nullptr,
            .scratchData = { .deviceAddress = 0 }
        };

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        sizeInfo.pNext = nullptr;

        u32 maxPrimitiveCount = 0;
        for (const auto& buildRange : buildRanges) {
            maxPrimitiveCount = std::max(maxPrimitiveCount, buildRange.primitiveCount);
        }

        vkGetAccelerationStructureBuildSizesKHR(
            m_Device->GetDevice(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &maxPrimitiveCount,
            &sizeInfo
        );

        m_Buffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeInfo.accelerationStructureSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        });

        auto scratchBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeInfo.buildScratchSize,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        });

        VkAccelerationStructureCreateInfoKHR createInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .createFlags = 0,
            .buffer = m_Buffer->GetBuffer(),
            .offset = 0,
            .size = sizeInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .deviceAddress = 0
        };

        VK_CHECK(vkCreateAccelerationStructureKHR(m_Device->GetDevice(), &createInfo, nullptr, &m_AS));

        command->SubmitOnce(QueueType::Compute, [&](VkCommandBuffer cmd) {
            buildInfo.dstAccelerationStructure = m_AS;
            buildInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

            const VkAccelerationStructureBuildRangeInfoKHR* pBuildRanges = buildRanges.data();

            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRanges);
        });

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructure = m_AS
        };

        m_DeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_Device->GetDevice(), &addressInfo);
    }

    VulkanTLAS::VulkanTLAS(std::shared_ptr<VulkanDevice> device, const std::shared_ptr<VulkanCommandManager>& command, const std::vector<TLASInstance>& instances)
        : VulkanAccelerationStructure(device)
    {
        std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
        vkInstances.reserve(instances.size());

        for (const auto& instance : instances) {
            vkInstances.push_back(VkAccelerationStructureInstanceKHR {
                .transform = Utils::ToVkTransformMatrix(instance.transform),
                .instanceCustomIndex = instance.instanceCustomIndex,
                .mask = instance.mask,
                .instanceShaderBindingTableRecordOffset = instance.sbtOffset,
                .flags = instance.flags,
                .accelerationStructureReference = instance.blas->GetDeviceAddress()
            });
        }

        auto instanceBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        });
        instanceBuffer->Write(vkInstances.data(), sizeof(VkAccelerationStructureInstanceKHR) * instances.size());

        VkAccelerationStructureGeometryInstancesDataKHR instancesData {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .pNext = nullptr,
            .arrayOfPointers = VK_FALSE,
            .data = { .deviceAddress = instanceBuffer->GetDeviceAddress() }
        };

        VkAccelerationStructureGeometryKHR vkGeo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = { .instances = instancesData },
            .flags = 0
        };

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .pNext = nullptr,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .srcAccelerationStructure = VK_NULL_HANDLE,
            .dstAccelerationStructure = VK_NULL_HANDLE,
            .geometryCount = 1,
            .pGeometries = &vkGeo,
            .ppGeometries = nullptr,
            .scratchData = { .deviceAddress = 0 }
        };

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo {
            .primitiveCount = static_cast<u32>(instances.size()),
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        sizeInfo.pNext = nullptr;

        vkGetAccelerationStructureBuildSizesKHR(m_Device->GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &rangeInfo.primitiveCount, &sizeInfo);

        m_Buffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeInfo.accelerationStructureSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        });

        auto scratchBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeInfo.buildScratchSize,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        });

        VkAccelerationStructureCreateInfoKHR createInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .createFlags = 0,
            .buffer = m_Buffer->GetBuffer(),
            .offset = 0,
            .size = sizeInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .deviceAddress = 0
        };

        VK_CHECK(vkCreateAccelerationStructureKHR(m_Device->GetDevice(), &createInfo, nullptr, &m_AS));

        command->SubmitOnce(QueueType::Compute, [&](VkCommandBuffer cmd) {
            buildInfo.dstAccelerationStructure = m_AS;
            buildInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

            const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &rangeInfo;

            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);
        });

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructure = m_AS
        };

        m_DeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_Device->GetDevice(), &addressInfo);
    }

}
