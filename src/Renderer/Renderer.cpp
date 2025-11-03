#include "Renderer.hpp"

#include <stb_image_write.h>

#include "Core/Logger.hpp"
#include "RHI/VulkanDescriptorWriter.hpp"

namespace PathTracer {

    struct Vertex
    {
        glm::vec3 pos;
    };

    Renderer::Renderer(std::shared_ptr<Window> window)
        : m_Window(window)
    {
        m_Width = m_Window->GetWidth();
        m_Height = m_Window->GetHeight();

        m_Context = std::make_shared<VulkanContext>(m_Window);
        m_Device = std::make_shared<VulkanDevice>(m_Context);

        m_Swapchain = std::make_shared<VulkanSwapchain>(m_Context, m_Device);
        m_Swapchain->Recreate(m_Width, m_Height);

        m_Command = std::make_shared<VulkanCommand>(m_Device);

        m_StorageImage = std::make_shared<VulkanImage>(m_Device, VulkanImage::Spec {
            .width = m_Width,
            .height = m_Height,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        });

        m_StagingBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = m_Width * m_Height * 4,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        });

        static const std::vector<Vertex> vertices {
            {{  0.0f, -0.5f, 0.0f }},
            {{  0.5f,  0.5f, 0.0f }},
            {{ -0.5f,  0.5f, 0.0f }}
        };

        static const std::vector<u32> indices { 0, 1, 2 };

        m_VertexBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeof(Vertex) * vertices.size(),
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        });
        m_VertexBuffer->Write((void*)vertices.data(), sizeof(Vertex) * vertices.size());

        m_IndexBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeof(u32) * indices.size(),
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        });
        m_IndexBuffer->Write((void*)indices.data(), sizeof(u32) * indices.size());

        std::vector<VulkanBLAS::BLASGeometry> blasGeometries {
            {
                .vertexBuffer = m_VertexBuffer,
                .indexBuffer = m_IndexBuffer,
                .vertexCount = static_cast<u32>(vertices.size()),
                .indexCount = static_cast<u32>(indices.size()),
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexStride = sizeof(Vertex),
                .isOpaque = true
            }
        };

        m_BLAS = std::make_shared<VulkanBLAS>(m_Device, m_Command, blasGeometries);

        std::vector<VulkanTLAS::TLASInstance> tlasInstances {
            {
                .blas = m_BLAS,
                .transform = glm::mat4(1.0f),
                .instanceCustomIndex = 0,
                .mask = 0xFF,
                .sbtOffset = 0,
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
            }
        };

        m_TLAS = std::make_shared<VulkanTLAS>(m_Device, m_Command, tlasInstances);

        std::vector<VkDescriptorPoolSize> poolSizes {
            {
                .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                .descriptorCount = 1,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
            }
        };
        
        m_DescriptorPool = std::make_shared<VulkanDescriptorPool>(m_Device, 1, poolSizes);

        m_DescriptorSetLayout = VulkanDescriptorSetLayout::Builder(m_Device)
            .AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
            .Build();

        m_DescriptorSet = VulkanDescriptorWriter(m_Device, m_DescriptorSetLayout, m_DescriptorPool)
            .WriteAccelerationStructure(0, m_TLAS)
            .WriteImage(1, m_StorageImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)
            .Build().value();

        m_PipelineLayout = VulkanPipelineLayout::Builder(m_Device)
            .AddSetLayout(m_DescriptorSetLayout)
            .Build();

        m_Pipeline = VulkanRayTracingPipeline::Builder(m_Device, m_PipelineLayout)
            .AddRayGenShaderGroup(std::make_shared<VulkanShader>(m_Device, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "triangle.rgen.spv"))
            .AddMissShaderGroup(std::make_shared<VulkanShader>(m_Device, VK_SHADER_STAGE_MISS_BIT_KHR, "triangle.rmiss.spv"))
            .AddHitShaderGroup(std::make_shared<VulkanShader>(m_Device, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "triangle.rchit.spv"))
            .SetMaxDepth(8)
            .Build();

        m_SBT = VulkanShaderBindingTable::Builder(m_Device, m_Pipeline)
            .AddRayGenGroup()
            .AddMissGroup()
            .AddHitGroup()
            .Build();

        m_RenderThread = std::thread(&Renderer::RenderLoop, this);
    }

    Renderer::~Renderer()
    {
        m_Running = false;
        m_RenderCondition.notify_one();

        if (m_RenderThread.joinable())
            m_RenderThread.join();

        stbi_flip_vertically_on_write(true);

        if (stbi_write_png("output.png", m_Width, m_Height, 4, m_StagingBuffer->Map(), m_Width * 4)) {
            LOG_INFO("Render saved to output.png");
        } else {
            LOG_ERROR("Failed to save image");
        }

        m_StagingBuffer->Unmap();

    }

    void Renderer::RequestResize(u32 width, u32 height)
    {
        {
            std::lock_guard lock(m_ResizeMutex);
            m_ResizeRequest = { width, height };
            m_ResizeRequested = true;
        }

        m_RenderCondition.notify_one();
    }

    void Renderer::Submit(const RenderPacket& packet)
    {
        {
            std::lock_guard lock(m_SubmitMutex);
            m_RenderPacket = packet;
            m_PacketSubmitted = true;
        }

        m_RenderCondition.notify_one();
    }

    void Renderer::RenderLoop()
    {
        while (m_Running) {
            {
                std::unique_lock lock(m_RenderMutex);
                m_RenderCondition.wait(lock, [this] {
                    return !m_Running || m_ResizeRequested.load() || m_PacketSubmitted.load();
                });
            }

            if (!m_Running) break;

            if (m_ResizeRequested.load()) {
                static ResizeRequest request;
                {
                    std::lock_guard lock(m_ResizeMutex);
                    request = m_ResizeRequest;
                    m_ResizeRequested = false;
                }

                HandleResize(request);
            }

            if (m_PacketSubmitted.load()) {
                static RenderPacket packet;
                {
                    std::lock_guard lock(m_SubmitMutex);
                    packet = std::move(m_RenderPacket);
                    m_PacketSubmitted = false;
                }

                Draw(packet);
            }
        }
    }

    void Renderer::Draw(const RenderPacket& packet)
    {
        m_Command->SubmitOnce([&](VkCommandBuffer cmd) {
            VkImageMemoryBarrier imageBarrier {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_NONE,
                .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = m_StorageImage->GetImage(),
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageBarrier
            );

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline->GetPipeline());
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_PipelineLayout->GetLayout(), 0, 1, &m_DescriptorSet, 0, nullptr);

            VkStridedDeviceAddressRegionKHR rgenRegion = m_SBT->GetRayGenRegion();
            VkStridedDeviceAddressRegionKHR missRegion = m_SBT->GetMissRegion();
            VkStridedDeviceAddressRegionKHR hitRegion = m_SBT->GetHitRegion();
            VkStridedDeviceAddressRegionKHR callableRegion = m_SBT->GetCallbableRegion();

            vkCmdTraceRaysKHR(cmd, &rgenRegion, &missRegion, &hitRegion, &callableRegion, m_Width, m_Height, 1);

            imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageBarrier
            );

            VkBufferImageCopy region {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageOffset = { 0, 0, 0 },
                .imageExtent = { m_Width, m_Height, 1 }
            };

            vkCmdCopyImageToBuffer(cmd,
                m_StorageImage->GetImage(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_StagingBuffer->GetBuffer(),
                1, &region
            );
        });
    }

    void Renderer::HandleResize(const ResizeRequest& request)
    {
        if (request.width == 0 || request.height == 0)
            return;

        m_Width = request.width;
        m_Height = request.height;

        m_Swapchain->Recreate(request.width, request.height);
    }

}
