#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4100)
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <glm/glm.hpp>

#if defined(__clang__)
    #pragma clang diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include "Types.hpp"
#include "RHI/VulkanContext.hpp"
#include "RHI/VulkanDevice.hpp"
#include "RHI/VulkanCommand.hpp"
#include "RHI/VulkanImage.hpp"
#include "RHI/VulkanBuffer.hpp"
#include "RHI/VulkanAccelerationStructure.hpp"
#include "RHI/VulkanDescriptorSetLayout.hpp"
#include "RHI/VulkanPipelineLayout.hpp"
#include "RHI/VulkanShader.hpp"
#include "RHI/VulkanPipeline.hpp"
#include "RHI/VulkanDescriptorPool.hpp"
#include "RHI/VulkanDescriptorWriter.hpp"
#include "RHI/VulkanShaderBindingTable.hpp"

struct Vertex
{
    glm::vec3 pos;
};

// TODO: All this should be in some kind of Application class
using namespace PathTracer;

int main()
{
    constexpr u32 WIDTH = 1280;
    constexpr u32 HEIGHT = 720;

    std::vector<Vertex> vertices = {
        {{  0.0f, -0.5f, 0.0f }},
        {{  0.5f,  0.5f, 0.0f }},
        {{ -0.5f,  0.5f, 0.0f }}
    };

    std::vector<u32> indices = { 0, 1, 2 };

    auto context = std::make_shared<VulkanContext>();
    auto device = std::make_shared<VulkanDevice>(context);
    auto command = std::make_shared<VulkanCommand>(device);

    auto storageImage = std::make_shared<VulkanImage>(device, VulkanImage::Spec {
        .width = WIDTH,
        .height = HEIGHT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    });

    auto stagingBuffer = std::make_shared<VulkanBuffer>(device, VulkanBuffer::Spec {
        .size = WIDTH * HEIGHT * 4,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    });

    auto vertexBuffer = std::make_shared<VulkanBuffer>(device, VulkanBuffer::Spec {
        .size = sizeof(Vertex) * vertices.size(),
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ,
        .memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    });
    vertexBuffer->Write(vertices.data(), sizeof(Vertex) * vertices.size());

    auto indexBuffer = std::make_shared<VulkanBuffer>(device, VulkanBuffer::Spec {
        .size = sizeof(u32) * indices.size(),
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT,
        .memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    });
    indexBuffer->Write(indices.data(), sizeof(u32) * indices.size());

    std::vector<VulkanBLAS::BLASGeometry> blasGeometries {
        {
            .vertexBuffer = vertexBuffer,
            .indexBuffer = indexBuffer,
            .vertexCount = static_cast<u32>(vertices.size()),
            .indexCount = static_cast<u32>(indices.size()),
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexStride = sizeof(Vertex),
            .isOpaque = true
        }
    };

    auto blas = std::make_shared<VulkanBLAS>(device, command, blasGeometries);

    std::vector<VulkanTLAS::TLASInstance> tlasInstances {
        {
            .blas = blas,
            .transform = glm::mat4(1.0f),
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .sbtOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
        }
    };

    auto tlas = std::make_shared<VulkanTLAS>(device, command, tlasInstances);

    auto setLayout = VulkanDescriptorSetLayout::Builder(device)
        .AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .Build();

    std::vector<VkDescriptorPoolSize> poolSizes {
        {
            .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1
        }
    };

    auto descriptorPool = std::make_shared<VulkanDescriptorPool>(device, 1, poolSizes);

    VkDescriptorSet descriptorSet = VulkanDescriptorWriter(device, setLayout, descriptorPool)
        .WriteAccelerationStructure(0, tlas)
        .WriteImage(1, storageImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)
        .Build().value();

    auto pipelineLayout = VulkanPipelineLayout::Builder(device)
        .AddSetLayout(setLayout)
        .Build();

    auto pipeline = VulkanRayTracingPipeline::Builder(device, pipelineLayout)
        .AddRayGenShaderGroup(std::make_shared<VulkanShader>(device, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "triangle.rgen.spv"))
        .AddMissShaderGroup(std::make_shared<VulkanShader>(device, VK_SHADER_STAGE_MISS_BIT_KHR, "triangle.rmiss.spv"))
        .AddHitShaderGroup(std::make_shared<VulkanShader>(device, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "triangle.rchit.spv"))
        .SetMaxDepth(8)
        .Build();

    auto sbt = VulkanShaderBindingTable::Builder(device, pipeline)
        .AddRayGenGroup()
        .AddMissGroup()
        .AddHitGroup()
        .Build();

    command->SubmitOnce([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier imageBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = storageImage->GetImage(),
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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->GetPipeline());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout->GetLayout(), 0, 1, &descriptorSet, 0, nullptr);

        VkStridedDeviceAddressRegionKHR rgenRegion = sbt->GetRayGenRegion();
        VkStridedDeviceAddressRegionKHR missRegion = sbt->GetMissRegion();
        VkStridedDeviceAddressRegionKHR hitRegion = sbt->GetHitRegion();
        VkStridedDeviceAddressRegionKHR callableRegion = sbt->GetCallbableRegion();

        vkCmdTraceRaysKHR(cmd, &rgenRegion, &missRegion, &hitRegion, &callableRegion, WIDTH, HEIGHT, 1);

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
            .imageExtent = { WIDTH, HEIGHT, 1 }
        };

        vkCmdCopyImageToBuffer(cmd,
            storageImage->GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stagingBuffer->GetBuffer(),
            1, &region
        );
    });

    stbi_flip_vertically_on_write(true);

    void* image = stagingBuffer->Map();
    stbi_write_png("output.png", WIDTH, HEIGHT, 4, image, WIDTH * 4);
    stagingBuffer->Unmap();
}
