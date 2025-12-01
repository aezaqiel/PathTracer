#include "AccelerationStructure.hpp"

#include "Device.hpp"
#include "Buffer.hpp"

namespace RHI {

    namespace {

        inline VkTransformMatrixKHR ToVkMatrix(const glm::mat4& mat)
        {
            const glm::mat4 t = glm::transpose(mat);
            VkTransformMatrixKHR out;
            memcpy(&out, &t, sizeof(VkTransformMatrixKHR));
            return out;
        }

    }

    AccelerationStructure::AccelerationStructure(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    AccelerationStructure::~AccelerationStructure()
    {
        vkDestroyAccelerationStructureKHR(m_Device->GetDevice(), m_AS, nullptr);
    }

    BLAS::BLAS(const std::shared_ptr<Device>& device, CommandContext<QueueType::Compute>& queue, const std::span<Geometry>& geometries)
        : AccelerationStructure(device)
    {
        std::vector<VkAccelerationStructureGeometryKHR> vkGeometries;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;

        vkGeometries.reserve(geometries.size());
        ranges.reserve(geometries.size());

        for (const auto& geo : geometries) {
            vkGeometries.push_back(VkAccelerationStructureGeometryKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .pNext = nullptr,
                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                .geometry = {
                    .triangles = {
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                        .vertexFormat = geo.vertices.format,
                        .vertexData = { .deviceAddress = geo.vertices.buffer->GetDeviceAddress() },
                        .vertexStride = geo.vertices.stride,
                        .maxVertex = geo.vertices.count,
                        .indexType = VK_INDEX_TYPE_UINT32,
                        .indexData = { .deviceAddress = geo.indices.buffer->GetDeviceAddress() },
                        .transformData = { 0 }
                    }
                },
                .flags = geo.isOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0u
            });

            ranges.push_back(VkAccelerationStructureBuildRangeInfoKHR {
                .primitiveCount = geo.indices.count / 3,
                .primitiveOffset = static_cast<u32>(geo.indices.offset),
                .firstVertex = static_cast<u32>(geo.vertices.offset / geo.vertices.stride),
                .transformOffset = 0
            });
        }

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .pNext = nullptr,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .srcAccelerationStructure = VK_NULL_HANDLE,
            .dstAccelerationStructure = VK_NULL_HANDLE,
            .geometryCount = static_cast<u32>(vkGeometries.size()),
            .pGeometries = vkGeometries.data(),
            .ppGeometries = nullptr,
            .scratchData = { .deviceAddress = 0 }
        };

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructureSize = 0,
            .updateScratchSize = 0,
            .buildScratchSize = 0
        };

        std::vector<u32> maxPrimitiveCounts;
        maxPrimitiveCounts.reserve(ranges.size());
        for (const auto& range : ranges) {
            maxPrimitiveCounts.push_back(range.primitiveCount);
        }

        vkGetAccelerationStructureBuildSizesKHR(m_Device->GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, maxPrimitiveCounts.data(), &sizeInfo);

        u64 scratchAlignment = m_Device->GetASProps().minAccelerationStructureScratchOffsetAlignment;

        m_Buffer = std::make_unique<Buffer>(m_Device, Buffer::Spec {
            .size = sizeInfo.accelerationStructureSize + scratchAlignment,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        VkAccelerationStructureCreateInfoKHR asInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .createFlags = 0,
            .buffer = m_Buffer->GetBuffer(),
            .offset = 0,
            .size = sizeInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .deviceAddress = 0
        };
        VK_CHECK(vkCreateAccelerationStructureKHR(m_Device->GetDevice(), &asInfo, nullptr, &m_AS));

        Buffer scratch(m_Device, Buffer::Spec {
            .size = sizeInfo.buildScratchSize + scratchAlignment,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        buildInfo.dstAccelerationStructure = m_AS;
        buildInfo.scratchData.deviceAddress = VkUtils::AlignUp(scratch.GetDeviceAddress(), scratchAlignment);

        VkQueryPoolCreateInfo queryPoolInfo {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1,
            .pipelineStatistics = 0
        };

        VkQueryPool queryPool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateQueryPool(m_Device->GetDevice(), &queryPoolInfo, nullptr, &queryPool));

        VkCommandBuffer cmdBuffer = queue.Record([&](VkCommandBuffer cmd) {
            vkCmdResetQueryPool(cmd, queryPool, 0, 1);

            const VkAccelerationStructureBuildRangeInfoKHR* pRanges = ranges.data();
            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRanges);

            VkMemoryBarrier2 barrier {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
            };

            VkDependencyInfo dependency {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .dependencyFlags = 0,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &barrier,
                .bufferMemoryBarrierCount = 0,
                .pBufferMemoryBarriers = nullptr,
                .imageMemoryBarrierCount = 0,
                .pImageMemoryBarriers = nullptr
            };

            vkCmdPipelineBarrier2(cmd, &dependency);

            vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &m_AS, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
        });

        m_Device->Submit<QueueType::Compute>(cmdBuffer, {}, {});
        m_Device->SyncTimeline<QueueType::Compute>();

        VkDeviceSize compactedSize = 0;
        VK_CHECK(vkGetQueryPoolResults(m_Device->GetDevice(), queryPool, 0, 1, sizeof(VkDeviceSize), &compactedSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

        LOG_TRACE("BLAS Compaction: {} -> {} bytes ({:.2f}%)", sizeInfo.accelerationStructureSize, compactedSize, static_cast<f32>(compactedSize) / sizeInfo.accelerationStructureSize * 100.0f);

        auto compactBuffer = std::make_unique<Buffer>(m_Device, Buffer::Spec {
            .size = compactedSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        VkAccelerationStructureCreateInfoKHR compactInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .createFlags = 0,
            .buffer = compactBuffer->GetBuffer(),
            .offset = 0,
            .size = compactedSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .deviceAddress = 0
        };

        VkAccelerationStructureKHR compactAS = VK_NULL_HANDLE;
        VK_CHECK(vkCreateAccelerationStructureKHR(m_Device->GetDevice(), &compactInfo, nullptr, &compactAS));

        VkCommandBuffer compactCmd = queue.Record([&](VkCommandBuffer cmd) {
            VkCopyAccelerationStructureInfoKHR copyInfo {
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .pNext = nullptr,
                .src = m_AS,
                .dst = compactAS,
                .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
            };

            vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
        });

        m_Device->Submit<QueueType::Compute>(compactCmd, {}, {});
        m_Device->SyncTimeline<QueueType::Compute>();

        vkDestroyQueryPool(m_Device->GetDevice(), queryPool, nullptr);
        vkDestroyAccelerationStructureKHR(m_Device->GetDevice(), m_AS, nullptr);

        m_AS = std::move(compactAS);
        m_Buffer = std::move(compactBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructure = m_AS
        };

        m_Address = vkGetAccelerationStructureDeviceAddressKHR(m_Device->GetDevice(), &addressInfo);
    }

    TLAS::TLAS(const std::shared_ptr<Device>& device, CommandContext<QueueType::Compute>& queue, const std::span<Instance>& instances)
        : AccelerationStructure(device)
    {
        std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
        vkInstances.reserve(instances.size());

        for (const auto& instance : instances) {
            vkInstances.push_back(VkAccelerationStructureInstanceKHR {
                .transform = ToVkMatrix(instance.transform),
                .instanceCustomIndex = instance.instanceCustomIndex,
                .mask = instance.mask,
                .instanceShaderBindingTableRecordOffset = instance.sbtOffset,
                .flags = instance.flags,
                .accelerationStructureReference = instance.blas->GetAddress()
            });
        }

        VkDeviceSize instanceBufferSize = vkInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
        Buffer instanceBuffer(m_Device, Buffer::Spec {
            .size = instanceBufferSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        Buffer staging(m_Device, Buffer::Spec {
            .size = instanceBufferSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory = VMA_MEMORY_USAGE_CPU_ONLY
        });
        staging.Write(vkInstances.data(), instanceBufferSize);

        VkCommandBuffer cmdBuffer = queue.Record([&](VkCommandBuffer cmd) {
            VkBufferCopy copyRegion {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = instanceBufferSize
            };

            vkCmdCopyBuffer(cmd, staging.GetBuffer(), instanceBuffer.GetBuffer(), 1, &copyRegion);

            VkMemoryBarrier2 barrier {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
            };

            VkDependencyInfo dependency {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .dependencyFlags = 0,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &barrier,
                .bufferMemoryBarrierCount = 0,
                .pBufferMemoryBarriers = nullptr,
                .imageMemoryBarrierCount = 0,
                .pImageMemoryBarriers = nullptr
            };

            vkCmdPipelineBarrier2(cmd, &dependency);
        });

        m_Device->Submit<QueueType::Compute>(cmdBuffer, {}, {});
        m_Device->SyncTimeline<QueueType::Compute>();

        VkAccelerationStructureGeometryKHR geometry {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                    .pNext = nullptr,
                    .arrayOfPointers = VK_FALSE,
                    .data = { .deviceAddress = instanceBuffer.GetDeviceAddress() }
                }
            },
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
            .pGeometries = &geometry,
            .ppGeometries = nullptr,
            .scratchData = { .deviceAddress = 0 }
        };

        u32 count = static_cast<u32>(instances.size());

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructureSize = 0,
            .updateScratchSize = 0,
            .buildScratchSize = 0
        };

        vkGetAccelerationStructureBuildSizesKHR(m_Device->GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &count, &sizeInfo);

        u64 scratchAlignment = m_Device->GetASProps().minAccelerationStructureScratchOffsetAlignment;

        m_Buffer = std::make_unique<Buffer>(m_Device, Buffer::Spec {
            .size = sizeInfo.accelerationStructureSize + scratchAlignment,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        VkAccelerationStructureCreateInfoKHR asInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .createFlags = 0,
            .buffer = m_Buffer->GetBuffer(),
            .offset = 0,
            .size = sizeInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .deviceAddress = 0
        };

        VK_CHECK(vkCreateAccelerationStructureKHR(m_Device->GetDevice(), &asInfo, nullptr, &m_AS));

        Buffer scratch(m_Device, Buffer::Spec {
            .size = sizeInfo.buildScratchSize + scratchAlignment,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        buildInfo.dstAccelerationStructure = m_AS;
        buildInfo.scratchData.deviceAddress = VkUtils::AlignUp(scratch.GetDeviceAddress(), scratchAlignment);

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo {
            .primitiveCount = count,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

        cmdBuffer = queue.Record([&](VkCommandBuffer cmd) {
            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
        });

        m_Device->Submit<QueueType::Compute>(cmdBuffer, {}, {});
        m_Device->SyncTimeline<QueueType::Compute>();

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructure = m_AS
        };

        m_Address = vkGetAccelerationStructureDeviceAddressKHR(m_Device->GetDevice(), &addressInfo);
    }

}
