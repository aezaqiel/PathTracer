#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Device;

    struct DescriptorLayoutBuilder
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkDescriptorBindingFlags> flags;

        DescriptorLayoutBuilder(const std::shared_ptr<Device>& device);

        DescriptorLayoutBuilder& AddBinding(u32 binding, VkDescriptorType type, u32 count, VkShaderStageFlags stage, VkDescriptorBindingFlags flag = 0);
        VkDescriptorSetLayout Build();
    
    private:
        std::shared_ptr<Device> m_Device;
    };

    class DescriptorManager
    {
    public:
        DescriptorManager(const std::shared_ptr<Device>& device);
        ~DescriptorManager();

        u32 RegisterTexture(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        void UnregisterTexture(u32 index);

        u32 RegisterSampler(VkSampler sampler);
        void UnregisterSampler(u32 index);

        void Bind(VkCommandBuffer cmd, VkPipelineBindPoint bind, VkPipelineLayout layout, u32 index = 0);

        void UpdateStorageImage(u32 binding, VkImageView view, VkImageLayout layout);
        void UpdateTLAS(u32 binding, VkAccelerationStructureKHR tlas);

        VkDescriptorSetLayout GetLayout() const { return m_Layout; }
        VkDescriptorSet GetSet() const { return m_Set; }

    private:
        void InitLayout();
        void InitPool();

    private:
        inline static constexpr u32 s_MaxBindlessTextures { 4096 };
        inline static constexpr u32 s_MaxBindlessSamplers { 64 };

        std::shared_ptr<Device> m_Device;

        VkDescriptorSetLayout m_Layout { VK_NULL_HANDLE };
        VkDescriptorPool m_Pool { VK_NULL_HANDLE };
        VkDescriptorSet m_Set { VK_NULL_HANDLE };

        std::queue<u32> m_FreeTextureIndices;
        std::queue<u32> m_FreeSamplerIndices;
        std::mutex m_TextureMutex;
        std::mutex m_SamplerMutex;
    };

}
