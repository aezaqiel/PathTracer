#include "Logger.hpp"

#include "RHI/Instance.hpp"
#include "RHI/Device.hpp"
#include "RHI/CommandManager.hpp"
#include "RHI/Buffer.hpp"
#include "RHI/Image.hpp"
#include "RHI/AccelerationStructure.hpp"

#include "Scene/SceneLoader.hpp"

constexpr u32 WIDTH = 1280;
constexpr u32 HEIGHT = 720;
constexpr u32 SAMPLES = 32;

int main()
{
    Logger::Init();

    RHI::Instance instance;
    RHI::Device device(instance);

    RHI::CommandManager<RHI::QueueType::Graphics> graphics(device);
    RHI::CommandManager<RHI::QueueType::Compute> compute(device);
    RHI::CommandManager<RHI::QueueType::Transfer> transfer(device);

    auto model = Scene::GlTFLoader::Load("../assets/Suzanne.glb");
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
    std::vector<RHI::TLAS::Instance> tlasInstances;

    for (const auto& obj : model->renderables) {
        const auto& primitive = model->primitives[obj.primitiveIndex];

        RHI::BLAS::Geometry geometry {
            .vertices = {
                .buffer = &vertexBuffer,
                .count = primitive.indexCount,
                .stride = sizeof(Scene::Vertex),
                .offset = primitive.vertexOffset * sizeof(Scene::Vertex),
                .format = VK_FORMAT_R32G32B32_SFLOAT
            },
            .indices = {
                .buffer = &indexBuffer,
                .count = primitive.indexCount,
                .offset = primitive.indexOffset * sizeof(u32)
            },
            .isOpaque = true
        };

        std::vector<RHI::BLAS::Geometry> geometries = { geometry };
        blases.push_back(std::move(std::make_unique<RHI::BLAS>(device, compute, geometries)));

        tlasInstances.push_back(RHI::TLAS::Instance {
            .blas = blases.back().get(),
            .transform = obj.transform,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .sbtOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
        });
    }

    RHI::TLAS tlas(device, compute, tlasInstances);

    // RAYTRACING PASSES
    for (u32 s = 0; s < SAMPLES; ++s) {
    }

    // GRAPHICS PASS (POSTPROCESSING)

    Logger::Shutdown();
}
