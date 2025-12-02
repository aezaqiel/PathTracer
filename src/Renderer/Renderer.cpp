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

Renderer::Renderer(const std::shared_ptr<Window>& window, const Settings& settings)
    :   m_Window(window),
        m_Width(settings.width),
        m_Height(settings.height)
{
    m_Instance = std::make_shared<RHI::Instance>(window);
    m_Device = std::make_shared<RHI::Device>(m_Instance);

    m_Swapchain = std::make_unique<RHI::Swapchain>(m_Instance, m_Device);
    m_Swapchain->Create(window->GetWidth(), window->GetHeight());

    m_GraphicsCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Graphics>>(m_Device);
    m_ComputeCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Compute>>(m_Device);
    m_TransferCommand = std::make_unique<RHI::CommandContext<RHI::QueueType::Transfer>>(m_Device);

    m_BindlessHeap = std::make_unique<RHI::BindlessHeap>(m_Device);

    for (auto& frame : m_FrameData) {
        frame.camera = std::make_unique<RHI::Buffer>(m_Device, RHI::Buffer::Spec {
            .size = sizeof(Scene::CameraData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
        });

        frame.GBuffer = {
            .albedo = std::make_unique<RHI::Image>(m_Device, RHI::Image::Spec {
                .extent = { m_Width, m_Height, 1 },
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .memory = VMA_MEMORY_USAGE_GPU_ONLY
            }),
            .normal = std::make_unique<RHI::Image>(m_Device, RHI::Image::Spec {
                .extent = { m_Width, m_Height, 1 },
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .memory = VMA_MEMORY_USAGE_GPU_ONLY
            }),
            .position = std::make_unique<RHI::Image>(m_Device, RHI::Image::Spec {
                .extent = { m_Width, m_Height, 1 },
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .memory = VMA_MEMORY_USAGE_GPU_ONLY
            }),
            .depth = std::make_unique<RHI::Image>(m_Device, RHI::Image::Spec {
                .extent = { m_Width, m_Height, 1 },
                .format = VK_FORMAT_D32_SFLOAT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .memory = VMA_MEMORY_USAGE_GPU_ONLY
            }),
        };
    }

    m_GBufferLayout = RHI::DescriptorLayoutBuilder(m_Device)
        .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    std::vector<VkFormat> GBufferFormats {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT
    };

    m_GBufferPipeline = RHI::GraphicsPipelineBuilder(m_Device)
        .SetVertexShader(s_ShaderPath / "gbuffer.vert.spv")
        .SetFragmentShader(s_ShaderPath / "gbuffer.frag.spv")
        .SetColorFormats(GBufferFormats)
        .SetDepthFormat(VK_FORMAT_D32_SFLOAT)
        .SetDepthTest(true, true)
        .AddLayout(m_BindlessHeap->GetLayout())
        .AddLayout(m_GBufferLayout)
        .AddPushConstant(sizeof(Scene::CameraData) + sizeof(u32), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    LoadScene();
}

Renderer::~Renderer()
{
    m_Device->WaitIdle();

    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_GBufferLayout, nullptr);
}

void Renderer::Draw(Scene::CameraData&& cam)
{
    if (m_ResizeRequested) {
        RecreateSwapchain();
        m_ResizeRequested = false;
    }

    auto& frame = m_FrameData[m_Device->GetCurrentFrameIndex()];
    frame.camera->Write(&cam, sizeof(Scene::CameraData));

    // m_Device->SyncFrame();

    // if (auto result = m_Swapchain->AcquireNextImage()) {
    //     if (*result == VK_ERROR_OUT_OF_DATE_KHR) {
    //         RecreateSwapchain();
    //         return;
    //     }
    // }

    // if (auto result = m_Swapchain->Present()) {
    //     if (*result == VK_ERROR_OUT_OF_DATE_KHR) RecreateSwapchain();
    // }
}

void Renderer::OnEvent(const Event& event)
{
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<WindowResizedEvent>([&](const WindowResizedEvent& e) {
        m_Width = e.width;
        m_Height = e.height;

        m_ResizeRequested = true;
    });
}

void Renderer::LoadScene()
{
    auto model = Scene::GlTFLoader::Load(s_AssetPath / "Suzanne.glb");

    m_VertexBuffer = RHI::Buffer::Stage(
        m_Device, *m_TransferCommand,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        model->vertices.size() * sizeof(Scene::Vertex),
        model->vertices.data(),
        m_Device->GetQueueFamily<RHI::QueueType::Graphics>()
    );

    m_IndexBuffer = RHI::Buffer::Stage(
        m_Device, *m_TransferCommand,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        model->indices.size() * sizeof(u32),
        model->indices.data(),
        m_Device->GetQueueFamily<RHI::QueueType::Graphics>()
    );

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
                m_Device->GetQueueFamily<RHI::QueueType::Graphics>()
            );
        });

        VkCommandBuffer acquireCmd = m_GraphicsCommand->Record([&](VkCommandBuffer cmd) {
            image->TransitionLayout(cmd,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE,
                VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_SHADER_READ_BIT,
                m_Device->GetQueueFamily<RHI::QueueType::Transfer>(),
                m_Device->GetQueueFamily<RHI::QueueType::Graphics>()
            );
        });

        std::vector<VkSemaphoreSubmitInfo> signal {
            m_Device->Submit<RHI::QueueType::Transfer>(transferCmd, {}, {})
        };

        m_Device->Submit<RHI::QueueType::Graphics>(acquireCmd, signal, {});
        m_Device->SyncTimeline<RHI::QueueType::Graphics>();

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

    m_MaterialBuffer = RHI::Buffer::Stage(
        m_Device, *m_TransferCommand,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        gpuMaterials.size() * sizeof(GPUMaterial),
        gpuMaterials.data(),
        m_Device->GetQueueFamily<RHI::QueueType::Graphics>()
    );

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

    m_ObjectDescBuffer = RHI::Buffer::Stage(
        m_Device, *m_TransferCommand,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        renderObjs.size() * sizeof(RenderObject),
        renderObjs.data(),
        m_Device->GetQueueFamily<RHI::QueueType::Graphics>()
    );

    VkCommandBuffer acquireCmd = m_GraphicsCommand->Record([&](VkCommandBuffer cmd) {
        std::vector<VkBufferMemoryBarrier2> barriers;

        auto AddBarrier = [&](VkBuffer buffer, VkDeviceSize size) {
            barriers.push_back(VkBufferMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask = VK_ACCESS_2_NONE,
                .srcQueueFamilyIndex = m_Device->GetQueueFamily<RHI::QueueType::Transfer>(),
                .dstQueueFamilyIndex = m_Device->GetQueueFamily<RHI::QueueType::Graphics>(),
                .buffer = buffer,
                .offset = 0,
                .size = size
            });
        };

        AddBarrier(m_VertexBuffer->GetBuffer(), m_VertexBuffer->GetSize());
        AddBarrier(m_IndexBuffer->GetBuffer(), m_IndexBuffer->GetSize());
        AddBarrier(m_MaterialBuffer->GetBuffer(), m_MaterialBuffer->GetSize());
        AddBarrier(m_ObjectDescBuffer->GetBuffer(), m_ObjectDescBuffer->GetSize());

        VkDependencyInfo dependency {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = static_cast<u32>(barriers.size()),
            .pBufferMemoryBarriers = barriers.data(),
            .imageMemoryBarrierCount = 0,
            .pImageMemoryBarriers = nullptr
        };

        vkCmdPipelineBarrier2(cmd, &dependency);
    });

    m_Device->Submit<RHI::QueueType::Graphics>(acquireCmd, {}, {});
    m_Device->SyncTimeline<RHI::QueueType::Graphics>();
}

void Renderer::RecreateSwapchain() const
{
    m_Device->WaitIdle();
    m_Swapchain->Create(m_Width, m_Height);
}
