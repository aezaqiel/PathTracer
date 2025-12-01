#pragma once

#include "Core/Events.hpp"

#include "RHI/Instance.hpp"
#include "RHI/Device.hpp"
#include "RHI/Swapchain.hpp"
#include "RHI/CommandContext.hpp"
#include "RHI/Buffer.hpp"
#include "RHI/Texture.hpp"
#include "RHI/AccelerationStructure.hpp"
#include "RHI/DescriptorManager.hpp"
#include "RHI/Pipeline.hpp"

#include "Scene/Camera.hpp"

class Window;

class Renderer
{
public:
    struct Settings
    {
        u32 width;
        u32 height;
        u32 samples;
        u32 tile;
    };

public:
    Renderer(const std::shared_ptr<Window>& window, const Settings& settings);
    ~Renderer();

    void Draw(Scene::CameraData&& cam);
    void OnEvent(const Event& event);

private:
    void LoadScene();
    void RecreateSwapchain() const;

private:
    template <typename T>
    using PerFrame = std::array<T, RHI::Device::GetFrameInFlight()>;

    std::shared_ptr<Window> m_Window;

    u32 m_Width { 0 };
    u32 m_Height { 0 };
    u32 m_Samples { 0 };
    u32 m_TileSize { 0 };

    bool m_ResizeRequested { false };

    std::shared_ptr<RHI::Instance> m_Instance;
    std::shared_ptr<RHI::Device> m_Device;
    std::unique_ptr<RHI::Swapchain> m_Swapchain;

    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Graphics>> m_GraphicsCommand;
    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Compute>> m_ComputeCommand;
    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Transfer>> m_TransferCommand;

    std::unique_ptr<RHI::BindlessHeap> m_BindlessHeap;

    PerFrame<std::unique_ptr<RHI::Texture>> m_StorageTextures;
    PerFrame<std::unique_ptr<RHI::Buffer>> m_CamBuffers;

    VkDescriptorSetLayout m_RTLayout { VK_NULL_HANDLE };
    VkDescriptorSetLayout m_GLayout { VK_NULL_HANDLE };

    std::unique_ptr<RHI::GraphicsPipeline> m_GraphicsPipeline;
    std::unique_ptr<RHI::RayTracingPipelne> m_RayTracingPipeline;

    std::unique_ptr<RHI::Buffer> m_VertexBuffer;
    std::unique_ptr<RHI::Buffer> m_IndexBuffer;

    std::vector<std::unique_ptr<RHI::Texture>> m_SceneTextures;
    std::unique_ptr<RHI::Buffer> m_MaterialBuffer;
    std::unique_ptr<RHI::Buffer> m_ObjectDescBuffer;

    std::vector<std::unique_ptr<RHI::BLAS>> m_BLASes;
    std::unique_ptr<RHI::TLAS> m_TLAS;
};
