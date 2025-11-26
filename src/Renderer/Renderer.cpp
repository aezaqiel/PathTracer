#include "Renderer.hpp"

#include "Core/Window.hpp"

#include "Scene/SceneLoader.hpp"
#include "PathConfig.inl"

Renderer::Renderer(const std::shared_ptr<Window>& window)
    : m_Window(window)
{
    m_Instance = std::make_shared<RHI::Instance>(window);
    m_Device = std::make_shared<RHI::Device>(m_Instance);

    m_Swapchain = std::make_unique<RHI::Swapchain>(m_Instance, m_Device);
    m_Swapchain->Create(window->GetWidth(), window->GetHeight());

    m_Graphics = std::make_unique<RHI::CommandContext<RHI::QueueType::Graphics>>(m_Device);
    m_Compute = std::make_unique<RHI::CommandContext<RHI::QueueType::Compute>>(m_Device);
    m_Transfer = std::make_unique<RHI::CommandContext<RHI::QueueType::Transfer>>(m_Device);

    std::filesystem::path assetPath = PathConfig::ASSET_DIR;
    auto model = Scene::GlTFLoader::Load(assetPath / "Suzanne.glb");

    m_VertexBuffer = std::make_unique<RHI::Buffer>(m_Device, RHI::Buffer::Spec {
        .size = model->vertices.size() * sizeof(Scene::Vertex),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
    });
    m_VertexBuffer->Write(model->vertices.data(), m_VertexBuffer->GetSize());

    m_IndexBuffer = std::make_unique<RHI::Buffer>(m_Device, RHI::Buffer::Spec {
        .size = model->indices.size() * sizeof(u32),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
    });
    m_IndexBuffer->Write(model->indices.data(), m_IndexBuffer->GetSize());

    m_BLASes.reserve(model->meshes.size());

    for (const auto& mesh : model->meshes) {
        std::vector<RHI::BLAS::Geometry> geometries;
        geometries.reserve(mesh.primitives.size());

        for (const auto& primitive : mesh.primitives) {
            geometries.push_back(RHI::BLAS::Geometry {
                .vertices = {
                    .buffer = m_VertexBuffer.get(),
                    .count = static_cast<u32>(model->vertices.size()),
                    .stride = sizeof(Scene::Vertex),
                    .offset = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT
                },
                .indices = {
                    .buffer = m_IndexBuffer.get(),
                    .count = primitive.indexCount,
                    .offset = primitive.indexOffset * sizeof(u32)
                },
                .isOpaque = true
            });
        }

        m_BLASes.push_back(std::make_unique<RHI::BLAS>(m_Device, *m_Compute, geometries));
    }

    std::vector<RHI::TLAS::Instance> tlasInstances;
    tlasInstances.reserve(model->nodes.size());

    for (const auto& node : model->nodes) {
        tlasInstances.push_back(RHI::TLAS::Instance {
            .blas = m_BLASes[node.meshIndex].get(),
            .transform = node.transform,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .sbtOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
        });
    }

    m_TLAS = std::make_unique<RHI::TLAS>(m_Device, *m_Compute, tlasInstances);
}

Renderer::~Renderer()
{
    m_Device->WaitIdle();
}

void Renderer::Draw()
{
    m_Device->SyncFrame();

    if (auto result = m_Swapchain->AcquireNextImage()) {
        if (*result == VK_ERROR_OUT_OF_DATE_KHR) RecreateSwapchain();
    }

    VkCommandBuffer computeCmd = m_Compute->Record([&](VkCommandBuffer cmd) {
    });

    VkCommandBuffer graphicsCmd = m_Graphics->Record([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier2 preRenderBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = m_Swapchain->GetCurrentImage(),
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VkDependencyInfo preRenderDependency {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &preRenderBarrier
        };
        vkCmdPipelineBarrier2(cmd, &preRenderDependency);

        VkClearValue clearValue = {{{ 0.1f, 0.1f, 0.1f, 1.0f }}};

        VkRenderingAttachmentInfo colorAttachment {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = m_Swapchain->GetCurrentImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearValue
        };

        VkRenderingInfo renderingInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { 0, 0 }, m_Swapchain->GetExtent() },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
        };

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdEndRendering(cmd);

        VkImageMemoryBarrier2 postRenderBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = m_Swapchain->GetCurrentImage(),
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VkDependencyInfo postRenderDependency {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &postRenderBarrier
        };
        vkCmdPipelineBarrier2(cmd, &postRenderDependency);
    });

    VkSemaphoreSubmitInfo computeSignal = m_Device->Submit<RHI::QueueType::Compute>(computeCmd, {}, {});

    std::vector<VkSemaphoreSubmitInfo> graphicsWait {
        computeSignal,
        m_Swapchain->GetAcquireWaitInfo()
    };

    std::vector<VkSemaphoreSubmitInfo> graphicsSignal {
        m_Swapchain->GetPresentSignalInfo()
    };

    m_Device->Submit<RHI::QueueType::Graphics>(graphicsCmd, graphicsWait, graphicsSignal);

    if (auto result = m_Swapchain->Present()) {
        if (*result == VK_ERROR_OUT_OF_DATE_KHR) RecreateSwapchain();
    }
}

void Renderer::RecreateSwapchain() const
{
    m_Device->WaitIdle();
    m_Swapchain->Create(m_Window->GetWidth(), m_Window->GetHeight());
}
