#include "Buffer.hpp"

#include "Device.hpp"

namespace RHI {

    Buffer::Buffer(const std::shared_ptr<Device>& device, const Spec& spec)
        : m_Device(device), m_Size(spec.size)
    {
        VkBufferCreateInfo bufferInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = m_Size,
            .usage = spec.usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        VmaAllocationCreateInfo allocationInfo;
        memset(&allocationInfo, 0, sizeof(VmaAllocationCreateInfo));
        allocationInfo.usage = spec.memory;

        if (spec.memory == VMA_MEMORY_USAGE_AUTO_PREFER_HOST || spec.memory == VMA_MEMORY_USAGE_CPU_TO_GPU || spec.memory == VMA_MEMORY_USAGE_GPU_TO_CPU) {
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VK_CHECK(vmaCreateBuffer(m_Device->GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));

        if (spec.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            VkBufferDeviceAddressInfo addressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = nullptr,
                .buffer = m_Buffer
            };

            m_DeviceAddress = vkGetBufferDeviceAddress(m_Device->GetDevice(), &addressInfo);
        }
    }

    Buffer::~Buffer()
    {
        if (m_MappedData) Unmap();
        vmaDestroyBuffer(m_Device->GetAllocator(), m_Buffer, m_Allocation);
    }

    void* Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
    {
        if (m_MappedData) {
            LOG_WARN("Buffer memory alreay mapped, unmapping old data...");
            Unmap();
        }

        VK_CHECK(vmaMapMemory(m_Device->GetAllocator(), m_Allocation, &m_MappedData));

        return reinterpret_cast<void*>(reinterpret_cast<u8*>(m_MappedData) + offset);
    }

    void Buffer::Unmap()
    {
        if (!m_MappedData) {
            LOG_WARN("No memory mapped to buffer");
            return;
        }

        vmaUnmapMemory(m_Device->GetAllocator(), m_Allocation);
        m_MappedData = nullptr;
    }

    void Buffer::Write(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        Map(size, offset);
        memcpy(m_MappedData, data, static_cast<usize>(size));
        Unmap();
    }

    std::unique_ptr<Buffer> Buffer::Stage(
        const std::shared_ptr<Device>& device,
        CommandContext<QueueType::Transfer>& transfer,
        VkBufferUsageFlags usage,
        VkDeviceSize size,
        const void* data,
        u32 dstQueueFamily
    )
    {
        Buffer staging(device, Spec {
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory = VMA_MEMORY_USAGE_CPU_ONLY
        });
        staging.Write(data, size);

        auto buffer = std::make_unique<Buffer>(device, Spec {
            .size = size,
            .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        VkCommandBuffer stagingCmd = transfer.Record([&](VkCommandBuffer cmd) {
            VkBufferCopy copyRegion {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = size
            };

            vkCmdCopyBuffer(cmd, staging.GetBuffer(), buffer->GetBuffer(), 1, &copyRegion);

            VkBufferMemoryBarrier2 barrier {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask = VK_ACCESS_2_NONE,
                .srcQueueFamilyIndex = device->GetQueueFamily<QueueType::Transfer>(),
                .dstQueueFamilyIndex = dstQueueFamily,
                .buffer = buffer->GetBuffer(),
                .offset = 0,
                .size = size
            };

            VkDependencyInfo dependency {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .dependencyFlags = 0,
                .memoryBarrierCount = 0,
                .pMemoryBarriers = nullptr,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers = &barrier,
                .imageMemoryBarrierCount = 0,
                .pImageMemoryBarriers = nullptr
            };

            vkCmdPipelineBarrier2(cmd, &dependency);
        });

        device->Submit<QueueType::Transfer>(stagingCmd, {}, {});
        device->SyncTimeline<QueueType::Transfer>();

        return std::move(buffer);
    }

}
