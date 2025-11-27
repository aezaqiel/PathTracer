#pragma once

#include "VkTypes.hpp"
#include "Shader.hpp"

namespace RHI {

    class Device;
    class DescriptorManager;
    class Shader;
    class Buffer;

    class Pipeline
    {
    public:
        virtual ~Pipeline();

        inline VkPipelineLayout GetLayout() const { return m_Layout; }

        virtual void Bind(VkCommandBuffer cmd) = 0;

    protected:
        Pipeline(const std::shared_ptr<Device>& device);

    protected:
        std::shared_ptr<Device> m_Device;

        VkPipeline m_Pipeline { VK_NULL_HANDLE };
        VkPipelineLayout m_Layout { VK_NULL_HANDLE };
    };

    class GraphicsPipeline : public Pipeline
    {
        friend class GraphicsPipelineBuilder;
    public:
        void Bind(VkCommandBuffer cmd) override;

        void SetViewport(VkCommandBuffer cmd, VkViewport viewport);
        void SetScissor(VkCommandBuffer cmd, VkRect2D scissor);

    protected:
        GraphicsPipeline(const std::shared_ptr<Device>& device);
    };

    class GraphicsPipelineBuilder
    {
    public:
        GraphicsPipelineBuilder(const std::shared_ptr<Device>& device, const std::shared_ptr<DescriptorManager>& descriptor);

        GraphicsPipelineBuilder& SetVertexShader(const std::filesystem::path& path);
        GraphicsPipelineBuilder& SetFragmentShader(const std::filesystem::path& path);

        GraphicsPipelineBuilder& SetColorFormats(const std::span<VkFormat>& formats);
        GraphicsPipelineBuilder& SetDepthFormat(VkFormat format);

        GraphicsPipelineBuilder& SetInputTopology(VkPrimitiveTopology topology);
        GraphicsPipelineBuilder& SetPolygonMode(VkPolygonMode mode);
        GraphicsPipelineBuilder& SetCullMode(VkCullModeFlags mode, VkFrontFace face = VK_FRONT_FACE_COUNTER_CLOCKWISE);
        GraphicsPipelineBuilder& SetDepthTest(bool enabled, bool write, VkCompareOp compare = VK_COMPARE_OP_LESS);
        GraphicsPipelineBuilder& SetBlending(bool enabled);

        GraphicsPipelineBuilder& AddPushConstant(u32 size, VkShaderStageFlags stage = VK_SHADER_STAGE_ALL);

        std::unique_ptr<GraphicsPipeline> Build();

    private:
        std::shared_ptr<Device> m_Device;
        std::shared_ptr<DescriptorManager> m_Descriptor;

        std::unique_ptr<Shader> m_VertexShader;
        std::unique_ptr<Shader> m_FragmentShader;

        std::vector<VkFormat> m_ColorFormats;
        VkFormat m_DepthFormat { VK_FORMAT_UNDEFINED };

        VkPipelineInputAssemblyStateCreateInfo m_InputAssembly {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        VkPipelineRasterizationStateCreateInfo m_Rasterizer {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f
        };

        VkPipelineMultisampleStateCreateInfo m_Multisampling {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE
        };

        VkPipelineDepthStencilStateCreateInfo m_DepthStencil {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE
        };

        VkPipelineColorBlendAttachmentState m_ColorBlendAttachment {
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };

        std::vector<VkPushConstantRange> m_PushConstants;
    };

    class RayTracingPipelne : public Pipeline
    {
        friend class RayTracingPipelineBuilder;
    public:
        void Bind(VkCommandBuffer cmd) override;

        inline VkStridedDeviceAddressRegionKHR GetRGenRegion() const { return m_RGenRegion; }
        inline VkStridedDeviceAddressRegionKHR GetMissRegion() const { return m_MissRegion; }
        inline VkStridedDeviceAddressRegionKHR GetHitRegion() const { return m_HitRegion; }
        inline VkStridedDeviceAddressRegionKHR GetCallRegion() const { return m_CallRegion; }

    protected:
        RayTracingPipelne(const std::shared_ptr<Device>& device);

    protected:
        std::unique_ptr<Buffer> m_SBTBuffer;

        VkStridedDeviceAddressRegionKHR m_RGenRegion {};
        VkStridedDeviceAddressRegionKHR m_MissRegion {};
        VkStridedDeviceAddressRegionKHR m_HitRegion  {};
        VkStridedDeviceAddressRegionKHR m_CallRegion {};
    };

    class RayTracingPipelineBuilder
    {
    public:
        RayTracingPipelineBuilder(const std::shared_ptr<Device>& device, const std::shared_ptr<DescriptorManager>& descriptor);

        RayTracingPipelineBuilder& AddRayGenShader(const std::filesystem::path& path);
        RayTracingPipelineBuilder& AddMissShader(const std::filesystem::path& path);
        RayTracingPipelineBuilder& AddClosestHitShader(const std::filesystem::path& path);

        RayTracingPipelineBuilder& AddPushConstant(u32 size);

        std::unique_ptr<RayTracingPipelne> Build();

    private:
        std::shared_ptr<Device> m_Device;
        std::shared_ptr<DescriptorManager> m_Descriptor;

        std::vector<std::unique_ptr<Shader>> m_Shaders;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShaderGroups;
        std::vector<VkPushConstantRange> m_PushConstants;

        u32 m_RGenCount { 0 };
        u32 m_MissCount { 0 };
        u32 m_HitCount  { 0 };
    };

}
