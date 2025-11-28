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

    class DescriptorAllocator
    {
    public:
        struct PoolSize
        {
            VkDescriptorType type;
            f32 multiplier;
        };

    public:
        DescriptorAllocator(const std::shared_ptr<Device>& device);
        ~DescriptorAllocator();

        bool Allocate(VkDescriptorSetLayout layout, VkDescriptorSet& set);
        void Reset();

    private:
        VkDescriptorPool GrabPool();

    private:
        std::shared_ptr<Device> m_Device;

        VkDescriptorPool m_CurrentPool { VK_NULL_HANDLE };
        std::vector<VkDescriptorPool> m_UsedPools;
        std::vector<VkDescriptorPool> m_FreePools;
    };

    class DescriptorWriter
    {
    public:
        DescriptorWriter(const std::shared_ptr<Device>& device);

        DescriptorWriter& WriteImage(u32 binding, VkImageView view, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
        DescriptorWriter& WriteBuffer(u32 binding, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset, VkDescriptorType type);
        DescriptorWriter& WriteAS(u32 binding, VkAccelerationStructureKHR as);

        bool Build(VkDescriptorSet& set, VkDescriptorSetLayout layout, DescriptorAllocator& allocator);
        void Overwrite(VkDescriptorSet set);

    private:
        std::shared_ptr<Device> m_Device;

        std::deque<VkDescriptorImageInfo> m_ImageInfos;
        std::deque<VkDescriptorBufferInfo> m_BufferInfos;
        std::deque<VkWriteDescriptorSetAccelerationStructureKHR> m_ASInfos;
        std::deque<VkAccelerationStructureKHR> m_ASes;
        std::vector<VkWriteDescriptorSet> m_Writes;
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

        VkDescriptorSetLayout GetGlobalLayout() const { return m_Layout; }
        VkDescriptorSet GetGlobalSet() const { return m_Set; }

        DescriptorAllocator& GetAllocator() const { return *m_Allocator; }

    private:
        void InitLayout();
        void InitPool();

    private:
        inline static constexpr u32 s_MaxBindlessTextures { 4096 };
        inline static constexpr u32 s_MaxBindlessSamplers { 64 };

        std::shared_ptr<Device> m_Device;
        std::unique_ptr<DescriptorAllocator> m_Allocator;

        VkDescriptorSetLayout m_Layout { VK_NULL_HANDLE };
        VkDescriptorPool m_Pool { VK_NULL_HANDLE };
        VkDescriptorSet m_Set { VK_NULL_HANDLE };

        std::queue<u32> m_FreeTextureIndices;
        std::queue<u32> m_FreeSamplerIndices;
        std::mutex m_TextureMutex;
        std::mutex m_SamplerMutex;
    };

}
