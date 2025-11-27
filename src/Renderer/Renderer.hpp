#pragma once

#include "RHI/Instance.hpp"
#include "RHI/Device.hpp"
#include "RHI/Swapchain.hpp"
#include "RHI/CommandContext.hpp"
#include "RHI/Buffer.hpp"
#include "RHI/Image.hpp"
#include "RHI/AccelerationStructure.hpp"
#include "RHI/DescriptorManager.hpp"

class Window;

class Renderer
{
public:
    Renderer(const std::shared_ptr<Window>& window);
    ~Renderer();

    void Draw();

private:
    void RecreateSwapchain() const;

private:
    std::shared_ptr<Window> m_Window;

    std::shared_ptr<RHI::Instance> m_Instance;
    std::shared_ptr<RHI::Device> m_Device;
    std::unique_ptr<RHI::Swapchain> m_Swapchain;

    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Graphics>> m_Graphics;
    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Compute>> m_Compute;
    std::unique_ptr<RHI::CommandContext<RHI::QueueType::Transfer>> m_Transfer;

    std::unique_ptr<RHI::DescriptorManager> m_DescriptorManager;

    std::unique_ptr<RHI::Buffer> m_VertexBuffer;
    std::unique_ptr<RHI::Buffer> m_IndexBuffer;

    std::vector<std::unique_ptr<RHI::BLAS>> m_BLASes;
    std::unique_ptr<RHI::TLAS> m_TLAS;
};
