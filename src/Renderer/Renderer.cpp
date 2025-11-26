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

    m_Graphics = std::make_unique<RHI::CommandManager<RHI::QueueType::Graphics>>(m_Device);
    m_Compute = std::make_unique<RHI::CommandManager<RHI::QueueType::Compute>>(m_Device);
    m_Transfer = std::make_unique<RHI::CommandManager<RHI::QueueType::Transfer>>(m_Device);

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
}

void Renderer::Draw()
{
}

void Renderer::RecreateSwapchain() const
{
    m_Device->WaitIdle();
    m_Swapchain->Create(m_Window->GetWidth(), m_Window->GetHeight());
}
