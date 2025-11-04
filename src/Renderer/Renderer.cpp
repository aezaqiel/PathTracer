#include "Renderer.hpp"

#include <stb_image_write.h>

#include "Core/Logger.hpp"
#include "RHI/VulkanDescriptorWriter.hpp"

namespace PathTracer {

    struct CameraUBO
    {
        glm::mat4 inverseView { 1.0f };
        glm::mat4 inverseProj { 1.0f };
        glm::vec3 position { 0.0f };
    };

    static constexpr u32 s_MaxSceneTextures = 256;

    Renderer::Renderer(std::shared_ptr<Window> window)
        : m_Window(window)
    {
        m_Width = m_Window->GetWidth();
        m_Height = m_Window->GetHeight();

        m_Context = std::make_shared<VulkanContext>(m_Window);
        m_Device = std::make_shared<VulkanDevice>(m_Context);

        m_Swapchain = std::make_shared<VulkanSwapchain>(m_Context, m_Device);
        m_Swapchain->Recreate(m_Width, m_Height);

        m_CommandManager = std::make_shared<VulkanCommandManager>(m_Device, m_Swapchain);

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

        m_CameraBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = sizeof(CameraUBO),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        });

        std::vector<VkDescriptorPoolSize> poolSizes {
            {
                .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                .descriptorCount = 1,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1
            },
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = s_MaxSceneTextures
            }
        };
        
        m_DescriptorPool = std::make_shared<VulkanDescriptorPool>(m_Device, 1, poolSizes);

        m_DescriptorSetLayout = VulkanDescriptorSetLayout::Builder(m_Device)
            .AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
            .AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, s_MaxSceneTextures)
            .Build();

        m_PipelineLayout = VulkanPipelineLayout::Builder(m_Device)
            .AddSetLayout(m_DescriptorSetLayout)
            .Build();

        m_Pipeline = VulkanRayTracingPipeline::Builder(m_Device, m_PipelineLayout)
            .AddRayGenShaderGroup(std::make_shared<VulkanShader>(m_Device, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "triangle.rgen.spv"))
            .AddMissShaderGroup(std::make_shared<VulkanShader>(m_Device, VK_SHADER_STAGE_MISS_BIT_KHR, "triangle.rmiss.spv"))
            .AddHitShaderGroup(std::make_shared<VulkanShader>(m_Device, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "triangle.rchit.spv"))
            .SetMaxDepth(4)
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

        m_CommandManager->SubmitOnce(QueueType::Transfer, [&](VkCommandBuffer cmd) {
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

        if (stbi_write_png("output.png", m_Width, m_Height, 4, m_StagingBuffer->Map(), m_Width * 4)) {
            LOG_INFO("Render saved to output.png");
        } else {
            LOG_ERROR("Failed to save image");
        }

        m_StagingBuffer->Unmap();

        m_CommandManager->WaitIdle();

        DestroySamplers();
    }

    void Renderer::LoadScene(std::shared_ptr<Model> model)
    {
        m_CommandManager->WaitIdle();

        m_VertexBuffer.clear();
        m_IndexBuffer.clear();
        m_MaterialBuffer.reset();
        m_BLASes.clear();
        m_TLAS.reset();
        m_SceneTextures.clear();

        m_SceneTextures.reserve(model->GetTextureCount());
        for (const auto& tex : model->textures) {
            VkDeviceSize size = tex->GetWidth() * tex->GetHeight() * 4;
            auto staging = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
            });
            staging->Write((void*)tex->GetData(), size);

            auto texture = std::make_shared<VulkanImage>(m_Device, VulkanImage::Spec {
                .width = static_cast<u32>(tex->GetWidth()),
                .height = static_cast<u32>(tex->GetHeight()),
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            });

            m_CommandManager->SubmitOnce(QueueType::Transfer, [tex, texture, staging](VkCommandBuffer cmd) {
                VkImageMemoryBarrier barrier {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_NONE,
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = texture->GetImage(),
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
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );

                VkBufferImageCopy copy {
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
                    .imageExtent = {
                        .width = static_cast<u32>(tex->GetWidth()),
                        .height = static_cast<u32>(tex->GetHeight()),
                        .depth = 1
                    }
                };
                vkCmdCopyBufferToImage(cmd, staging->GetBuffer(), texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_NONE;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );
            });

            m_SceneTextures.push_back(texture);
        }

        CreateSamplers(m_SceneTextures.size());

        VkDeviceSize matBufferSize = sizeof(Material) * model->GetMaterialCount();
        m_MaterialBuffer = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
            .size = matBufferSize,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        });
        m_MaterialBuffer->Write((void*)model->materials.data(), matBufferSize);

        m_VertexBuffer.reserve(model->GetMeshCount());
        m_IndexBuffer.reserve(model->GetMeshCount());

        for (const auto& mesh : model->meshes) {
            VkDeviceSize vertexSize = sizeof(Vertex) * mesh.vertices.size();
            auto vertexStaging = std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
                .size = vertexSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
            });
            vertexStaging->Write((void*)mesh.vertices.data(), vertexSize);

            VkDeviceSize indexSize = sizeof(u32) * mesh.indices.size();
            auto indexStaging = std::make_unique<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
                .size = indexSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
            });
            indexStaging->Write((void*)mesh.indices.data(), indexSize);

            auto& vb = m_VertexBuffer.emplace_back(std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
                .size = vertexSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            }));

            auto& ib = m_IndexBuffer.emplace_back(std::make_shared<VulkanBuffer>(m_Device, VulkanBuffer::Spec {
                .size = indexSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            }));

            m_CommandManager->SubmitOnce(QueueType::Transfer, [&](VkCommandBuffer cmd) {
                VkBufferCopy copy {
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = vertexSize
                };
                vkCmdCopyBuffer(cmd, vertexStaging->GetBuffer(), vb->GetBuffer(), 1, &copy);

                copy.size = indexSize;
                vkCmdCopyBuffer(cmd, indexStaging->GetBuffer(), ib->GetBuffer(), 1, &copy);
            });
        }

        std::vector<VulkanTLAS::TLASInstance> tlasInstances;

        for (usize i = 0; i < model->GetMeshCount(); ++i) {
            const auto& mesh = model->meshes[i];

            std::vector<VulkanBLAS::BLASGeometry> blasGeom {
                {
                    .vertexBuffer = m_VertexBuffer.at(i),
                    .indexBuffer = m_IndexBuffer.at(i),
                    .vertexCount = static_cast<u32>(mesh.vertices.size()),
                    .indexCount = static_cast<u32>(mesh.indices.size()),
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexStride = sizeof(Vertex),
                    .isOpaque = true
                }
            };

            auto& blas = m_BLASes.emplace_back(std::make_shared<VulkanBLAS>(m_Device, m_CommandManager, blasGeom));

            tlasInstances.push_back(VulkanTLAS::TLASInstance {
                .blas = blas,
                .transform = glm::mat4(1.0f),
                .instanceCustomIndex = mesh.materialIndex,
                .mask = 0xFF,
                .sbtOffset = 0,
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
            });
        }

        m_TLAS = std::make_shared<VulkanTLAS>(m_Device, m_CommandManager, tlasInstances);

        m_DescriptorPool->Reset();

        m_DescriptorSet = VulkanDescriptorWriter(m_Device, m_DescriptorSetLayout, m_DescriptorPool)
            .WriteAccelerationStructure(0, m_TLAS)
            .WriteImage(1, m_StorageImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)
            .WriteBuffer(2, m_CameraBuffer, 0)
            .WriteBuffer(3, m_MaterialBuffer, 0)
            .WriteImageArray(4, m_SceneTextures, m_SceneSamplers)
            .Build().value();
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
        if (packet.camera) {
            CameraUBO ubo {
                .inverseView = glm::inverse(packet.camera->GetViewMatrix()),
                .inverseProj = glm::inverse(packet.camera->GetProjMatrix()),
                .position = packet.camera->GetPosition()
            };

            m_CameraBuffer->Write(&ubo, sizeof(CameraUBO));
        }

        if (const auto& frame = m_CommandManager->BeginFrame()) {
            VkCommandBufferBeginInfo beginInfo
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pInheritanceInfo = nullptr
            };

            { // Transfer Commands
                VK_CHECK(vkBeginCommandBuffer(frame->transferBuffer, &beginInfo));
                VK_CHECK(vkEndCommandBuffer(frame->transferBuffer));
            }

            { // Compute Commands
                VK_CHECK(vkBeginCommandBuffer(frame->computeBuffer, &beginInfo));

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

                vkCmdPipelineBarrier(frame->computeBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &imageBarrier
                );

                vkCmdBindPipeline(frame->computeBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline->GetPipeline());
                vkCmdBindDescriptorSets(frame->computeBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_PipelineLayout->GetLayout(), 0, 1, &m_DescriptorSet, 0, nullptr);

                VkStridedDeviceAddressRegionKHR rgenRegion = m_SBT->GetRayGenRegion();
                VkStridedDeviceAddressRegionKHR missRegion = m_SBT->GetMissRegion();
                VkStridedDeviceAddressRegionKHR hitRegion = m_SBT->GetHitRegion();
                VkStridedDeviceAddressRegionKHR callableRegion = m_SBT->GetCallbableRegion();

                vkCmdTraceRaysKHR(frame->computeBuffer, &rgenRegion, &missRegion, &hitRegion, &callableRegion, m_Width, m_Height, 1);

                imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

                vkCmdPipelineBarrier(frame->computeBuffer,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &imageBarrier
                );

                VK_CHECK(vkEndCommandBuffer(frame->computeBuffer));
            }

            { // Graphics Commands
                VK_CHECK(vkBeginCommandBuffer(frame->graphicsBuffer, &beginInfo));

                VkImageMemoryBarrier swapchainImageBarrier {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_NONE,
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = m_Swapchain->GetImage(frame->imageIndex),
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                };

                vkCmdPipelineBarrier(frame->graphicsBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &swapchainImageBarrier
                );

                VkImageCopy copyRegion {
                    .srcSubresource = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    },
                    .srcOffset = { 0, 0, 0 },
                    .dstSubresource = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    },
                    .dstOffset = { 0, 0, 0 },
                    .extent = { m_Width, m_Height, 1 }
                };

                vkCmdCopyImage(frame->graphicsBuffer,
                    m_StorageImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    m_Swapchain->GetImage(frame->imageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &copyRegion
                );

                swapchainImageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                swapchainImageBarrier.dstAccessMask = VK_ACCESS_NONE;
                swapchainImageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                swapchainImageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

                vkCmdPipelineBarrier(frame->graphicsBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &swapchainImageBarrier
                );

                VK_CHECK(vkEndCommandBuffer(frame->graphicsBuffer));
            }

            m_CommandManager->EndFrame();
        }
    }

    void Renderer::HandleResize(const ResizeRequest& request)
    {
        if (request.width == 0 || request.height == 0)
            return;

        m_Width = request.width;
        m_Height = request.height;

        m_CommandManager->WaitIdle();

        m_Swapchain->Recreate(m_Width, m_Height);

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

        m_DescriptorPool->Reset();
        m_DescriptorSet = VulkanDescriptorWriter(m_Device, m_DescriptorSetLayout, m_DescriptorPool)
            .WriteAccelerationStructure(0, m_TLAS)
            .WriteImage(1, m_StorageImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)
            .WriteBuffer(2, m_CameraBuffer, 0)
            .WriteBuffer(3, m_MaterialBuffer, 0)
            .WriteImageArray(4, m_SceneTextures, m_SceneSamplers)
            .Build().value();
    }

    void Renderer::CreateSamplers(u32 count)
    {
        if (count == 0) return;

        DestroySamplers();
        m_SceneSamplers.resize(count);

        VkPhysicalDeviceProperties props = m_Device->GetProps().properties;

        VkSamplerCreateInfo samplerInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = props.limits.maxSamplerAnisotropy,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = VK_LOD_CLAMP_NONE,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };

        for (u32 i = 0; i < count; ++i) {
            VK_CHECK(vkCreateSampler(m_Device->GetDevice(), &samplerInfo, nullptr, &m_SceneSamplers[i]));
        }
    }

    void Renderer::DestroySamplers()
    {
        for (auto& sampler : m_SceneSamplers) {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_Device->GetDevice(), sampler, nullptr);
            }
        }
        m_SceneSamplers.clear();
    }

}
