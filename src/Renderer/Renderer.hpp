#pragma once

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
    Renderer(const std::shared_ptr<Window>& window);
    ~Renderer();

    void Draw(Scene::CameraData&& cam);

private:
    void LoadScene();
    void RecreateSwapchain() const;

private:
    std::shared_ptr<Window> m_Window;

    std::shared_ptr<RHI::Instance> m_Instance;
    std::shared_ptr<RHI::Device> m_Device;
    std::unique_ptr<RHI::Swapchain> m_Swapchain;

    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Graphics>> m_GraphicsCommand;
    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Compute>> m_ComputeCommand;
    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Transfer>> m_TransferCommand;

    std::unique_ptr<RHI::Texture> m_StorageTexture;
    std::unique_ptr<RHI::Buffer> m_CameraBuffer;

    std::unique_ptr<RHI::BindlessHeap> m_BindlessHeap;
    VkDescriptorSetLayout m_DrawLayout { VK_NULL_HANDLE };

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
