#include "RHI/Instance.hpp"
#include "RHI/Device.hpp"
#include "RHI/CommandManager.hpp"
#include "RHI/Buffer.hpp"
#include "RHI/Image.hpp"
#include "RHI/AccelerationStructure.hpp"

#include "Scene/SceneLoader.hpp"
#include "PathConfig.inl"

constexpr u32 WIDTH = 1280;
constexpr u32 HEIGHT = 720;
constexpr u32 SAMPLES = 32;

int main()
{
    Logger::Init();

    std::shared_ptr<RHI::Instance> instance = std::make_shared<RHI::Instance>();
    std::shared_ptr<RHI::Device> device = std::make_shared<RHI::Device>(instance);

    std::unique_ptr<RHI::CommandManager<RHI::QueueType::Graphics>> graphics = std::make_unique<RHI::CommandManager<RHI::QueueType::Graphics>>(device);
    std::unique_ptr<RHI::CommandManager<RHI::QueueType::Compute>> compute = std::make_unique<RHI::CommandManager<RHI::QueueType::Compute>>(device);
    std::unique_ptr<RHI::CommandManager<RHI::QueueType::Transfer>> transfer = std::make_unique<RHI::CommandManager<RHI::QueueType::Transfer>>(device);

    std::filesystem::path assetPath = PathConfig::ASSET_DIR;

    auto model = Scene::GlTFLoader::Load(assetPath / "Suzanne.glb");
    if (!model) return EXIT_FAILURE;

    RHI::Buffer vertexBuffer(device, RHI::Buffer::Spec {
        .size = model->vertices.size() * sizeof(Scene::Vertex),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
    });
    vertexBuffer.Write(model->vertices.data(), vertexBuffer.GetSize());

    RHI::Buffer indexBuffer(device, RHI::Buffer::Spec {
        .size = model->indices.size() * sizeof(u32),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
    });
    indexBuffer.Write(model->indices.data(), indexBuffer.GetSize());

    std::vector<std::unique_ptr<RHI::BLAS>> blases;
    blases.reserve(model->meshes.size());

    for (const auto& mesh : model->meshes) {
        std::vector<RHI::BLAS::Geometry> geometries;
        geometries.reserve(mesh.primitives.size());

        for (const auto& primitive : mesh.primitives) {
            geometries.push_back(RHI::BLAS::Geometry {
                .vertices = {
                    .buffer = &vertexBuffer,
                    .count = static_cast<u32>(model->vertices.size()),
                    .stride = sizeof(Scene::Vertex),
                    .offset = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT
                },
                .indices = {
                    .buffer = &indexBuffer,
                    .count = primitive.indexCount,
                    .offset = primitive.indexOffset * sizeof(u32)
                },
                .isOpaque = true
            });
        }

        blases.push_back(std::make_unique<RHI::BLAS>(device, *compute, geometries));
    }

    std::vector<RHI::TLAS::Instance> tlasInstances;
    tlasInstances.reserve(model->nodes.size());

    for (const auto& node : model->nodes) {
        tlasInstances.push_back(RHI::TLAS::Instance {
            .blas = blases[node.meshIndex].get(),
            .transform = node.transform,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .sbtOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
        });
    }

    std::unique_ptr<RHI::TLAS> tlas = std::make_unique<RHI::TLAS>(device, *compute, tlasInstances);

    // RAYTRACING PASSES
    for (u32 s = 0; s < SAMPLES; ++s) {
    }

    // GRAPHICS PASS (POSTPROCESSING)

    Logger::Shutdown();
}
