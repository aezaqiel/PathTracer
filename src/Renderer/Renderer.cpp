#include "Renderer.hpp"

#include "Core/Window.hpp"

#include "Scene/SceneLoader.hpp"
#include "PathConfig.inl"

Renderer::Renderer(const std::shared_ptr<Window>& window)
    : m_Window(window)
{
    std::filesystem::path shaderPath = PathConfig::ShaderDir;
    std::filesystem::path assetPath = PathConfig::AssetDir;

    m_Instance = std::make_shared<RHI::Instance>(window);
    m_Device = std::make_shared<RHI::Device>(m_Instance);

    m_Swapchain = std::make_unique<RHI::Swapchain>(m_Instance, m_Device);
    m_Swapchain->Create(window->GetWidth(), window->GetHeight());

    m_GraphicsCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Graphics>>(m_Device);
    m_ComputeCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Compute>>(m_Device);
    m_TransferCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Transfer>>(m_Device);

    m_DescriptorManager = std::make_shared<RHI::DescriptorManager>(m_Device);
    for (auto& allocator : m_DescriptorAllocators) {
        allocator = std::make_unique<RHI::DescriptorAllocator>(m_Device);
    }

    auto storageImage = std::make_shared<RHI::Image>(m_Device, RHI::Image::Spec {
        .extent = { window->GetWidth(), window->GetHeight(), 1 },
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .memory = VMA_MEMORY_USAGE_GPU_ONLY
    });

    auto storageSampler = std::make_shared<RHI::Sampler>(m_Device, RHI::Sampler::Spec {
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxAnisotropy = m_Device->GetProps().properties.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK
    });

    m_StorageTexture = std::make_unique<RHI::Texture>(storageImage, storageSampler);

    m_DescriptorManager->UpdateStorageImage(2, storageImage->GetView(), VK_IMAGE_LAYOUT_GENERAL);

    m_StorageTexture->SetBindlessIndices(
        m_DescriptorManager->RegisterTexture(storageImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        m_DescriptorManager->RegisterSampler(storageSampler->GetSampler())
    );

    std::vector<VkFormat> colorFormats = { m_Swapchain->GetFormat() };

    m_GraphicsPipeline = RHI::GraphicsPipelineBuilder(m_Device, m_DescriptorManager)
        .SetVertexShader(shaderPath / "post.vert.spv")
        .SetFragmentShader(shaderPath / "post.frag.spv")
        .SetColorFormats(colorFormats)
        .SetDepthTest(false, false)
        .SetDepthFormat(VK_FORMAT_UNDEFINED)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE)
        .AddPushConstant(sizeof(u32) * 2, VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    m_RayTracingPipeline = RHI::RayTracingPipelineBuilder(m_Device, m_DescriptorManager)
        .AddRayGenShader(shaderPath / "raygen.rgen.spv")
        .AddMissShader(shaderPath / "miss.rmiss.spv")
        .AddClosestHitShader(shaderPath / "closesthit.rchit.spv")
        .Build();

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

        m_BLASes.push_back(std::make_unique<RHI::BLAS>(m_Device, *m_ComputeCommand, geometries));
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

    m_TLAS = std::make_unique<RHI::TLAS>(m_Device, *m_ComputeCommand, tlasInstances);

    m_DescriptorManager->UpdateTLAS(3, m_TLAS->GetAS());
}

Renderer::~Renderer()
{
    m_Device->WaitIdle();
}

void Renderer::Draw()
{
    m_Device->SyncFrame();

    auto& allocator = m_DescriptorAllocators[m_Device->GetCurrentFrameIndex()];
    allocator->Reset();

    if (auto result = m_Swapchain->AcquireNextImage()) {
        if (*result == VK_ERROR_OUT_OF_DATE_KHR) RecreateSwapchain();
    }

    VkCommandBuffer computeCmd = m_ComputeCommand->Record([&](VkCommandBuffer cmd) {
        auto storageImage = m_StorageTexture->GetImage();

        storageImage->TransitionLayout(cmd,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
        );

        m_RayTracingPipeline->Bind(cmd);
        m_DescriptorManager->Bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RayTracingPipeline->GetLayout());

        auto rgen = m_RayTracingPipeline->GetRGenRegion();
        auto miss = m_RayTracingPipeline->GetMissRegion();
        auto hit = m_RayTracingPipeline->GetHitRegion();
        auto call = m_RayTracingPipeline->GetCallRegion();

        auto extent = storageImage->GetExtent();
        vkCmdTraceRaysKHR(cmd, &rgen, &miss, &hit, &call, extent.width, extent.height, extent.depth);

        storageImage->TransitionLayout(cmd,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_ACCESS_2_NONE
        );
    });

    VkCommandBuffer graphicsCmd = m_GraphicsCommand->Record([&](VkCommandBuffer cmd) {
        auto swapchainImage = m_Swapchain->GetCurrentImage();

        swapchainImage->TransitionLayout(cmd,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        );

        VkClearValue clearValue = {{{ 0.8f, 0.2f, 0.8f, 1.0f }}};

        VkRenderingAttachmentInfo colorAttachment {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchainImage->GetView(),
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

        m_GraphicsPipeline->Bind(cmd);
        m_DescriptorManager->Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetLayout());

        m_GraphicsPipeline->SetViewport(cmd, VkViewport {
            .x = 0.0f, .y = 0.0f,
            .width = static_cast<f32>(m_Swapchain->GetExtent().width),
            .height = static_cast<f32>(m_Swapchain->GetExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        });

        m_GraphicsPipeline->SetScissor(cmd, VkRect2D {
            .offset = VkOffset2D { 0, 0 },
            .extent = m_Swapchain->GetExtent()
        });

        u32 storageImageIndex = m_StorageTexture->GetImageIndex();
        u32 storageSamplerIndex = m_StorageTexture->GetSamplerIndex();

        vkCmdPushConstants(cmd, m_GraphicsPipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(u32), &storageImageIndex);
        vkCmdPushConstants(cmd, m_GraphicsPipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(u32), sizeof(u32), &storageSamplerIndex);

        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);

        swapchainImage->TransitionLayout(cmd,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_NONE
        );
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
