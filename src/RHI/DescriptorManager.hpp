#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Device;
    class Image;
    class Sampler;
    class Texture;

    class DescriptorLayoutBuilder
    {
    public:
        DescriptorLayoutBuilder(const std::shared_ptr<Device>& device);

        DescriptorLayoutBuilder& AddBinding(u32 binding, VkDescriptorType type, VkShaderStageFlags stage, u32 count = 1);
        DescriptorLayoutBuilder& AddBindlessBinding(u32 binding, VkDescriptorType type, VkShaderStageFlags stage, u32 count);

        VkDescriptorSetLayout Build();

    private:
        std::shared_ptr<Device> m_Device;

        std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
        std::vector<VkDescriptorBindingFlags> m_BindingFlags;
    };

    class DescriptorAllocator
    {
    public:
        struct PoolSizeRatio
        {
            VkDescriptorType type;
            f32 ratio;
        };

    public:
        DescriptorAllocator(const std::shared_ptr<Device>& device, u32 sets = 1000, std::span<PoolSizeRatio> ratios = {});
        ~DescriptorAllocator();

        VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

        void Reset();

    private:
        VkDescriptorPool GetPool();
        VkDescriptorPool CreatePool(u32 count, VkDescriptorPoolCreateFlags flags);

    private:
        std::shared_ptr<Device> m_Device;

        VkDescriptorPool m_CurrentPool { VK_NULL_HANDLE };
        std::vector<VkDescriptorPool> m_UsedPools;
        std::vector<VkDescriptorPool> m_FreePools;

        std::vector<PoolSizeRatio> m_Ratios;
        u32 m_SetsPerPool { 0 };
    };

    class BindlessHeap
    {
    public:
        BindlessHeap(const std::shared_ptr<Device>& device);
        ~BindlessHeap();

        u32 RegisterTexture(const Texture& texture);
        void UnregisterTexture(u32 index);

        inline VkDescriptorSet GetSet() const { return m_Set; }
        inline VkDescriptorSetLayout GetLayout() const { return m_Layout; }

        void Bind(VkCommandBuffer cmd, VkPipelineBindPoint bind, VkPipelineLayout layout);

    private:
        u32 AllocateIndex();
        void FreeIndex(u32 index);
        void UpdateTexture(u32 index, const Texture& texture);

    private:
        inline static constexpr u32 MAX_BINDLESS_RESOURCES { 1024 };

        std::shared_ptr<Device> m_Device;

        VkDescriptorSetLayout m_Layout { VK_NULL_HANDLE };
        VkDescriptorPool m_Pool { VK_NULL_HANDLE };
        VkDescriptorSet m_Set { VK_NULL_HANDLE };

        std::queue<u32> m_FreeIndices;
        std::mutex m_AllocationMutex;
    };

    class DescriptorWriter
    {
    public:
        DescriptorWriter() = default;

        DescriptorWriter& WriteImage(u32 binding, VkImageView view, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
        DescriptorWriter& WriteBuffer(u32 binding, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset, VkDescriptorType type);
        DescriptorWriter& WriteAS(u32 binding, VkAccelerationStructureKHR as);

        void Push(VkCommandBuffer cmd, VkPipelineBindPoint bind, VkPipelineLayout layout, u32 set);

    private:
        std::deque<VkDescriptorImageInfo> m_ImageInfos;
        std::deque<VkDescriptorBufferInfo> m_BufferInfos;

        std::deque<VkWriteDescriptorSetAccelerationStructureKHR> m_ASInfos;
        std::deque<VkAccelerationStructureKHR> m_ASes;

        std::vector<VkWriteDescriptorSet> m_Writes;
    };

}
