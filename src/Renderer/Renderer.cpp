#include "Renderer.hpp"

#include "Core/Window.hpp"

#include "Scene/SceneLoader.hpp"
#include "PathConfig.inl"

namespace {

    std::filesystem::path s_ShaderPath(PathConfig::ShaderDir);
    std::filesystem::path s_AssetPath(PathConfig::AssetDir);

    struct GPUMaterial
    {
        glm::vec4 baseColorFactor;
        glm::vec3 emissiveFactor;
        f32 metallicFactor;
        f32 roughnessFactor;
        f32 alphaCutOff;
        i32 alphaMode;
        i32 baseColorTexture;
        i32 metallicRoughnessTexture;
        i32 normalTexture;
        i32 occlusionTexture;
        i32 emissiveTexture;
        i32 _p[3];
    };

    struct RenderObject
    {
        u64 vertex;
        u64 index;
        u32 material;
        u32 _p;
    };

}

Renderer::Renderer(const std::shared_ptr<Window>& window)
    : m_Window(window)
{
    m_Instance = std::make_shared<RHI::Instance>(window);
    m_Device = std::make_shared<RHI::Device>(m_Instance);

    m_Swapchain = std::make_unique<RHI::Swapchain>(m_Instance, m_Device);
    m_Swapchain->Create(window->GetWidth(), window->GetHeight());

    m_GraphicsCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Graphics>>(m_Device);
    m_ComputeCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Compute>>(m_Device);
    m_TransferCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Transfer>>(m_Device);

    m_StorageTexture = std::make_unique<RHI::Texture>(
        std::make_shared<RHI::Image>(m_Device, RHI::Image::Spec {
            .extent = { window->GetWidth(), window->GetHeight(), 1 },
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        }),
        std::make_shared<RHI::Sampler>(m_Device, RHI::Sampler::Spec {
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .maxAnisotropy = m_Device->GetProps().properties.limits.maxSamplerAnisotropy,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK
        })
    );

    for (usize i = 0; i < RHI::Device::GetFrameInFlight(); ++i) {
        m_CamBuffers[i] = std::make_unique<RHI::Buffer>(m_Device, RHI::Buffer::Spec {
            .size = sizeof(Scene::CameraData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
        });
    }

    m_BindlessHeap = std::make_unique<RHI::BindlessHeap>(m_Device);
    m_BindlessHeap->RegisterTexture(*m_StorageTexture);

    m_DrawLayout = RHI::DescriptorLayoutBuilder(m_Device)
        .AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .Build();

    std::vector<VkFormat> colorFormats = { m_Swapchain->GetFormat() };

    m_GraphicsPipeline = RHI::GraphicsPipelineBuilder(m_Device)
        .SetVertexShader(s_ShaderPath / "post.vert.spv")
        .SetFragmentShader(s_ShaderPath / "post.frag.spv")
        .SetColorFormats(colorFormats)
        .SetDepthTest(false, false)
        .SetDepthFormat(VK_FORMAT_UNDEFINED)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE)
        .AddLayout(m_BindlessHeap->GetLayout())
        .AddPushConstant(sizeof(u32), VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    m_RayTracingPipeline = RHI::RayTracingPipelineBuilder(m_Device)
        .AddRayGenShader(s_ShaderPath / "raygen.rgen.spv")
        .AddMissShader(s_ShaderPath / "miss.rmiss.spv")
        .AddClosestHitShader(s_ShaderPath / "closesthit.rchit.spv")
        .AddLayout(m_BindlessHeap->GetLayout())
        .AddLayout(m_DrawLayout)
        .Build();

    LoadScene();
}

Renderer::~Renderer()
{
    m_Device->WaitIdle();

    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_DrawLayout, nullptr);
}

void Renderer::Draw(Scene::CameraData&& cam)
{
    m_Device->SyncFrame();

    if (auto result = m_Swapchain->AcquireNextImage()) {
        if (*result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain();
            return;
        }
    }

    auto& camBuffer = m_CamBuffers[m_Device->GetCurrentFrameIndex()];

    camBuffer->Write(&cam, sizeof(Scene::CameraData));

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
        m_BindlessHeap->Bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RayTracingPipeline->GetLayout());

        RHI::DescriptorWriter()
            .WriteAS(0, m_TLAS->GetAS())
            .WriteImage(1, m_StorageTexture->GetImage()->GetView(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .WriteBuffer(2, camBuffer->GetBuffer(), camBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .WriteBuffer(3, m_ObjectDescBuffer->GetBuffer(), m_ObjectDescBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .WriteBuffer(4, m_MaterialBuffer->GetBuffer(), m_MaterialBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .Push(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RayTracingPipeline->GetLayout(), 1);

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
        m_BindlessHeap->Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetLayout());

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

        vkCmdPushConstants(cmd, m_GraphicsPipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(u32), &storageImageIndex);

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

void Renderer::LoadScene()
{
    auto model = Scene::GlTFLoader::Load(s_AssetPath / "Sponza.glb");

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

    std::vector<u32> textures;
    textures.resize(model->textures.size());

    m_SceneTextures.reserve(model->textures.size());

    for (usize i = 0; i < model->textures.size(); ++i) {
        auto& tex = model->textures[i];

        VkFormat format;
        if (tex.channels == 4) format = VK_FORMAT_R8G8B8A8_UNORM;
        else if (tex.channels == 3) format = VK_FORMAT_R8G8B8_UNORM;
        else {
            LOG_WARN("Unsupported texture channel count: {}", tex.channels);
            continue;
        }

        auto image = std::make_shared<RHI::Image>(m_Device, RHI::Image::Spec {
            .extent = { tex.width, tex.height, 1 },
            .format = format,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .memory = VMA_MEMORY_USAGE_GPU_ONLY
        });

        VkDeviceSize imageSize = tex.pixels.size();
        RHI::Buffer staging(m_Device, RHI::Buffer::Spec {
            .size = imageSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory = VMA_MEMORY_USAGE_CPU_ONLY
        });
        staging.Write(reinterpret_cast<void*>(tex.pixels.data()), imageSize);

        VkCommandBuffer transferCmd = m_TransferCommand->Record([&](VkCommandBuffer cmd) {
            image->TransitionLayout(cmd,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_TRANSFER_WRITE_BIT
            );

            VkBufferImageCopy copyRegion {
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
                .imageExtent = image->GetExtent()
            };

            vkCmdCopyBufferToImage(cmd, staging.GetBuffer(), image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            image->TransitionLayout(cmd,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_ACCESS_2_NONE,
                m_Device->GetQueueFamily<RHI::QueueType::Transfer>(),
                m_Device->GetQueueFamily<RHI::QueueType::Compute>()
            );
        });

        VkCommandBuffer computeCmd = m_ComputeCommand->Record([&](VkCommandBuffer cmd) {
            image->TransitionLayout(cmd,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE,
                VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_SHADER_READ_BIT,
                m_Device->GetQueueFamily<RHI::QueueType::Transfer>(),
                m_Device->GetQueueFamily<RHI::QueueType::Compute>()
            );
        });

        auto semaphore = m_Device->Submit<RHI::QueueType::Transfer>(transferCmd, {}, {});

        std::vector<VkSemaphoreSubmitInfo> signal = { semaphore };
        m_Device->Submit<RHI::QueueType::Compute>(computeCmd, signal, {});

        m_Device->SyncTimeline<RHI::QueueType::Compute>();

        auto sampler = std::make_shared<RHI::Sampler>(m_Device, RHI::Sampler::Spec {
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .maxAnisotropy = m_Device->GetProps().properties.limits.maxSamplerAnisotropy,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK
        });

        auto texture = std::make_unique<RHI::Texture>(image, sampler);
        textures[i] = m_BindlessHeap->RegisterTexture(*texture);

        m_SceneTextures.push_back(std::move(texture));
    }

    std::vector<GPUMaterial> gpuMaterials;
    gpuMaterials.reserve(model->materials.size());

    for (const auto& mat : model->materials) {
        gpuMaterials.push_back(GPUMaterial {
            .baseColorFactor = mat.baseColorFactor,
            .emissiveFactor = mat.emissiveFactor,
            .metallicFactor = mat.metallicFactor,
            .roughnessFactor = mat.roughnessFactor,
            .alphaCutOff = mat.alphaCutoff,
            .alphaMode = static_cast<i32>(mat.alphaMode),
            .baseColorTexture = mat.baseColorTexture >= 0 ? static_cast<i32>(textures[mat.baseColorTexture]) : -1,
            .metallicRoughnessTexture = mat.metallicRoughnessTexture >= 0 ? static_cast<i32>(textures[mat.metallicRoughnessTexture]) : -1,
            .normalTexture = mat.normalTexture >= 0 ? static_cast<i32>(textures[mat.normalTexture]) : -1,
            .occlusionTexture = mat.occlusionTexture >= 0 ? static_cast<i32>(textures[mat.occlusionTexture]) : -1,
            .emissiveTexture = mat.emissiveTexture >= 0 ? static_cast<i32>(textures[mat.emissiveTexture]) : -1
        });
    }

    m_MaterialBuffer = std::make_unique<RHI::Buffer>(m_Device, RHI::Buffer::Spec {
        .size = gpuMaterials.size() * sizeof(GPUMaterial),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
    });
    m_MaterialBuffer->Write(gpuMaterials.data(), m_MaterialBuffer->GetSize());

    std::vector<RenderObject> renderObjs;
    std::vector<u32> objIndices;

    objIndices.reserve(model->meshes.size());

    VkDeviceAddress vertexAddress = m_VertexBuffer->GetDeviceAddress();
    VkDeviceAddress indexAddress = m_IndexBuffer->GetDeviceAddress();

    for (const auto& mesh : model->meshes) {
        objIndices.push_back(static_cast<u32>(renderObjs.size()));
        for (const auto& prim : mesh.primitives) {
            renderObjs.push_back(RenderObject {
                .vertex = vertexAddress + prim.vertexOffset * sizeof(Scene::Vertex),
                .index = indexAddress + prim.indexOffset * sizeof(u32),
                .material = prim.materialIndex
            });
        }
    }

    m_ObjectDescBuffer = std::make_unique<RHI::Buffer>(m_Device, RHI::Buffer::Spec {
        .size = renderObjs.size() * sizeof(RenderObject),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
    });
    m_ObjectDescBuffer->Write(renderObjs.data(), m_ObjectDescBuffer->GetSize());

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
            .instanceCustomIndex = objIndices[node.meshIndex],
            .mask = 0xFF,
            .sbtOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
        });
    }

    m_TLAS = std::make_unique<RHI::TLAS>(m_Device, *m_ComputeCommand, tlasInstances);
}

void Renderer::RecreateSwapchain() const
{
    m_Device->WaitIdle();
    m_Swapchain->Create(m_Window->GetWidth(), m_Window->GetHeight());
}
