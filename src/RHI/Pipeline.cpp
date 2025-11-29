#include "Pipeline.hpp"

#include "Device.hpp"
#include "Buffer.hpp"

namespace RHI {

    Pipeline::Pipeline(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    Pipeline::~Pipeline()
    {
        vkDestroyPipeline(m_Device->GetDevice(), m_Pipeline, nullptr);
        vkDestroyPipelineLayout(m_Device->GetDevice(), m_Layout, nullptr);
    }

    GraphicsPipeline::GraphicsPipeline(const std::shared_ptr<Device>& device)
        : Pipeline(device)
    {
    }

    void GraphicsPipeline::Bind(VkCommandBuffer cmd)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    }

    void GraphicsPipeline::SetViewport(VkCommandBuffer cmd, VkViewport viewport)
    {
        vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    void GraphicsPipeline::SetScissor(VkCommandBuffer cmd, VkRect2D scissor)
    {
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    GraphicsPipelineBuilder::GraphicsPipelineBuilder(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetVertexShader(const std::filesystem::path& path)
    {
        m_VertexShader = std::make_unique<Shader>(m_Device, path, Shader::Stage::Vertex);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetFragmentShader(const std::filesystem::path& path)
    {
        m_FragmentShader = std::make_unique<Shader>(m_Device, path, Shader::Stage::Fragment);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetColorFormats(const std::span<VkFormat>& formats)
    {
        m_ColorFormats.assign(formats.begin(), formats.end());
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthFormat(VkFormat format)
    {
        m_DepthFormat = format;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetInputTopology(VkPrimitiveTopology topology)
    {
        m_InputAssembly.topology = topology;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPolygonMode(VkPolygonMode mode)
    {
        m_Rasterizer.polygonMode = mode;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetCullMode(VkCullModeFlags mode, VkFrontFace face)
    {
        m_Rasterizer.cullMode = mode;
        m_Rasterizer.frontFace = face;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthTest(bool enabled, bool write, VkCompareOp compare)
    {
        m_DepthStencil.depthTestEnable = enabled ? VK_TRUE : VK_FALSE;
        m_DepthStencil.depthWriteEnable = write ? VK_TRUE : VK_FALSE;
        m_DepthStencil.depthCompareOp = compare;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetBlending(bool enabled)
    {
        m_ColorBlendAttachment.blendEnable = enabled ? VK_TRUE : VK_FALSE;
        if (enabled) {
            m_ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            m_ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            m_ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            m_ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            m_ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            m_ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddLayout(VkDescriptorSetLayout layout)
    {
        m_Layouts.push_back(layout);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddPushConstant(u32 size, VkShaderStageFlags stage)
    {
        m_PushConstants.push_back(VkPushConstantRange {
            .stageFlags = stage,
            .offset = 0,
            .size = size
        });
        return *this;
    }

    std::unique_ptr<GraphicsPipeline> GraphicsPipelineBuilder::Build()
    {
        auto pipeline = std::unique_ptr<GraphicsPipeline>(new GraphicsPipeline(m_Device));

        VkPipelineLayoutCreateInfo layoutInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<u32>(m_Layouts.size()),
            .pSetLayouts = m_Layouts.data(),
            .pushConstantRangeCount = static_cast<u32>(m_PushConstants.size()),
            .pPushConstantRanges = m_PushConstants.data()
        };

        VK_CHECK(vkCreatePipelineLayout(m_Device->GetDevice(), &layoutInfo, nullptr, &pipeline->m_Layout));

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        if (m_VertexShader) {
            shaderStages.push_back(VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = m_VertexShader->GetModule(),
                .pName = "main",
                .pSpecializationInfo = nullptr
            });
        }
        if (m_FragmentShader) {
            shaderStages.push_back(VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = m_FragmentShader->GetModule(),
                .pName = "main",
                .pSpecializationInfo = nullptr
            });
        }

        std::vector<VkDynamicState> dynamicStates {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        VkPipelineViewportStateCreateInfo viewportState {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = nullptr,
            .scissorCount = 1,
            .pScissors = nullptr
        };

        VkPipelineVertexInputStateCreateInfo vertexInputState {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr
        };

        std::vector<VkPipelineColorBlendAttachmentState> attachments(m_ColorFormats.size(), m_ColorBlendAttachment);
        VkPipelineColorBlendStateCreateInfo colorBlending {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = static_cast<u32>(attachments.size()),
            .pAttachments = attachments.data(),
            .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
        };

        VkPipelineRenderingCreateInfo renderingInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<u32>(m_ColorFormats.size()),
            .pColorAttachmentFormats = m_ColorFormats.data(),
            .depthAttachmentFormat = m_DepthFormat,
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
        };

        VkGraphicsPipelineCreateInfo pipelineInfo {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &renderingInfo,
            .flags = 0,
            .stageCount = static_cast<u32>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputState,
            .pInputAssemblyState = &m_InputAssembly,
            .pTessellationState = nullptr,
            .pViewportState = &viewportState,
            .pRasterizationState = &m_Rasterizer,
            .pMultisampleState = &m_Multisampling,
            .pDepthStencilState = &m_DepthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = pipeline->m_Layout,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VK_CHECK(vkCreateGraphicsPipelines(m_Device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline->m_Pipeline));

        return pipeline;
    }

    RayTracingPipelne::RayTracingPipelne(const std::shared_ptr<Device>& device)
        : Pipeline(device)
    {
    }

    void RayTracingPipelne::Bind(VkCommandBuffer cmd)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline);
    }

    RayTracingPipelineBuilder::RayTracingPipelineBuilder(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    RayTracingPipelineBuilder& RayTracingPipelineBuilder::AddRayGenShader(const std::filesystem::path& path)
    {
        m_Shaders.push_back(std::make_unique<Shader>(m_Device, path, Shader::Stage::RayGen));
        m_ShaderGroups.push_back(VkRayTracingShaderGroupCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<u32>(m_Shaders.size() - 1),
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        });
        m_RGenCount++;
        return *this;
    }

    RayTracingPipelineBuilder& RayTracingPipelineBuilder::AddMissShader(const std::filesystem::path& path)
    {
        m_Shaders.push_back(std::make_unique<Shader>(m_Device, path, Shader::Stage::Miss));
        m_ShaderGroups.push_back(VkRayTracingShaderGroupCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<u32>(m_Shaders.size() - 1),
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        });
        m_MissCount++;
        return *this;
    }

    RayTracingPipelineBuilder& RayTracingPipelineBuilder::AddClosestHitShader(const std::filesystem::path& path)
    {
        m_Shaders.push_back(std::make_unique<Shader>(m_Device, path, Shader::Stage::ClosestHit));
        m_ShaderGroups.push_back(VkRayTracingShaderGroupCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = static_cast<u32>(m_Shaders.size() - 1),
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        });
        m_HitCount++;
        return *this;
    }

    RayTracingPipelineBuilder& RayTracingPipelineBuilder::AddLayout(VkDescriptorSetLayout layout)
    {
        m_Layouts.push_back(layout);
        return *this;
    }

    RayTracingPipelineBuilder& RayTracingPipelineBuilder::AddPushConstant(u32 size)
    {
        m_PushConstants.push_back(VkPushConstantRange {
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset = 0,
            .size = size
        });
        return *this;
    }

    std::unique_ptr<RayTracingPipelne> RayTracingPipelineBuilder::Build()
    {
        auto pipeline = std::unique_ptr<RayTracingPipelne>(new RayTracingPipelne(m_Device));

        VkPipelineLayoutCreateInfo layoutInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<u32>(m_Layouts.size()),
            .pSetLayouts = m_Layouts.data(),
            .pushConstantRangeCount = static_cast<u32>(m_PushConstants.size()),
            .pPushConstantRanges = m_PushConstants.data()
        };

        VK_CHECK(vkCreatePipelineLayout(m_Device->GetDevice(), &layoutInfo, nullptr, &pipeline->m_Layout));

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        stages.reserve(m_Shaders.size());
        for (const auto& shader : m_Shaders) {
            stages.push_back(VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = shader->GetStage(),
                .module = shader->GetModule(),
                .pName = "main",
                .pSpecializationInfo = nullptr
            });
        }

        VkRayTracingPipelineCreateInfoKHR pipelineInfo {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = static_cast<u32>(stages.size()),
            .pStages = stages.data(),
            .groupCount = static_cast<u32>(m_ShaderGroups.size()),
            .pGroups = m_ShaderGroups.data(),
            .maxPipelineRayRecursionDepth = 8,
            .pLibraryInfo = nullptr,
            .pLibraryInterface = nullptr,
            .pDynamicState = nullptr,
            .layout = pipeline->m_Layout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VK_CHECK(vkCreateRayTracingPipelinesKHR(m_Device->GetDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline->m_Pipeline));

        const auto& rtProps = m_Device->GetRTProps();
        u32 handleSize = rtProps.shaderGroupHandleSize;
        u32 handleAlignment = rtProps.shaderGroupHandleAlignment;
        u32 baseAlignment = rtProps.shaderGroupBaseAlignment;

        u32 handleSizeAligned = AlignUp(handleSize, handleAlignment);

        pipeline->m_RGenRegion.stride = AlignUp(handleSizeAligned, baseAlignment);
        pipeline->m_RGenRegion.size = AlignUp(m_RGenCount * handleSizeAligned, baseAlignment);

        pipeline->m_MissRegion.stride = handleSizeAligned;
        pipeline->m_MissRegion.size = AlignUp(m_MissCount * handleSizeAligned, baseAlignment);

        pipeline->m_HitRegion.stride = handleSizeAligned;
        pipeline->m_HitRegion.size = AlignUp(m_HitCount * handleSizeAligned, baseAlignment);

        pipeline->m_CallRegion.stride = 0;
        pipeline->m_CallRegion.size = 0;

        u32 sbtSize = pipeline->m_RGenRegion.size + pipeline->m_MissRegion.size + pipeline->m_HitRegion.size + pipeline->m_CallRegion.size;

        std::vector<u8> handles(m_ShaderGroups.size() * handleSize);
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_Device->GetDevice(), pipeline->m_Pipeline, 0, static_cast<u32>(m_ShaderGroups.size()), handles.size(), handles.data()));

        pipeline->m_SBTBuffer = std::make_unique<Buffer>(m_Device, Buffer::Spec {
            .size = sbtSize,
            .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memory = VMA_MEMORY_USAGE_CPU_TO_GPU
        });

        VkDeviceAddress sbtAddress = pipeline->m_SBTBuffer->GetDeviceAddress();
        pipeline->m_RGenRegion.deviceAddress = sbtAddress;
        pipeline->m_MissRegion.deviceAddress = sbtAddress + pipeline->m_RGenRegion.size;
        pipeline->m_HitRegion.deviceAddress = pipeline->m_MissRegion.deviceAddress + pipeline->m_MissRegion.size;
        pipeline->m_CallRegion.deviceAddress = pipeline->m_HitRegion.deviceAddress + pipeline->m_HitRegion.size;

        u8* pSBTBuffer = static_cast<u8*>(pipeline->m_SBTBuffer->Map());
        u8* pData = handles.data();

        for (u32 i = 0; i < m_RGenCount; ++i) {
            memcpy(pSBTBuffer, pData, handleSize);
            pSBTBuffer += pipeline->m_RGenRegion.stride;
            pData += handleSize;
        }

        pipeline->m_SBTBuffer->Unmap();

        pSBTBuffer = static_cast<u8*>(pipeline->m_SBTBuffer->Map(VK_WHOLE_SIZE, pipeline->m_RGenRegion.size));
        for (u32 i = 0; i < m_MissCount; ++i) {
            memcpy(pSBTBuffer, pData, handleSize);
            pSBTBuffer += pipeline->m_MissRegion.stride;
            pData += handleSize;
        }

        pipeline->m_SBTBuffer->Unmap();

        pSBTBuffer = static_cast<u8*>(pipeline->m_SBTBuffer->Map(VK_WHOLE_SIZE, pipeline->m_RGenRegion.size + pipeline->m_MissRegion.size));
        for (u32 i = 0; i < m_MissCount; ++i) {
            memcpy(pSBTBuffer, pData, handleSize);
            pSBTBuffer += pipeline->m_HitRegion.stride;
            pData += handleSize;
        }

        pipeline->m_SBTBuffer->Unmap();

        return pipeline;
    }

}
