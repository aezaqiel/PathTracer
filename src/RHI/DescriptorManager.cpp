#include "DescriptorManager.hpp"

#include "Device.hpp"

namespace RHI {

    DescriptorLayoutBuilder::DescriptorLayoutBuilder(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBinding(u32 binding, VkDescriptorType type, u32 count, VkShaderStageFlags stage, VkDescriptorBindingFlags flag)
    {
        bindings.push_back(VkDescriptorSetLayoutBinding {
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = count,
            .stageFlags = stage,
            .pImmutableSamplers = nullptr
        });
        flags.push_back(flag);

        return *this;
    }

    VkDescriptorSetLayout DescriptorLayoutBuilder::Build()
    {
        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<u32>(flags.size()),
            .pBindingFlags = flags.data()
        };

        VkDescriptorSetLayoutCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &flagsInfo,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = static_cast<u32>(bindings.size()),
            .pBindings = bindings.data()
        };

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(m_Device->GetDevice(), &info, nullptr, &layout));

        return layout;
    }

    DescriptorManager::DescriptorManager(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
        InitLayout();
        InitPool();

        for (u32 i = 0; i < s_MaxBindlessTextures; ++i) m_FreeTextureIndices.push(i);
        for (u32 i = 0; i < s_MaxBindlessSamplers; ++i) m_FreeSamplerIndices.push(i);
    }

    DescriptorManager::~DescriptorManager()
    {
        vkDestroyDescriptorPool(m_Device->GetDevice(), m_Pool, nullptr);
        vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_Layout, nullptr);
    }

    u32 DescriptorManager::RegisterTexture(VkImageView view, VkImageLayout layout)
    {
        std::scoped_lock lock(m_TextureMutex);

        if (m_FreeTextureIndices.empty()) {
            LOG_ERROR("Bindless texture heap ran out of indices");
            return 0;
        }

        u32 index = m_FreeTextureIndices.front();
        m_FreeTextureIndices.pop();

        VkDescriptorImageInfo imageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = view,
            .imageLayout = layout
        };

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_Set,
            .dstBinding = 0,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);

        return index;
    }

    void DescriptorManager::UnregisterTexture(u32 index)
    {
        std::scoped_lock lock(m_TextureMutex);
        m_FreeTextureIndices.push(index);
    }

    u32 DescriptorManager::RegisterSampler(VkSampler sampler)
    {
        std::scoped_lock lock(m_SamplerMutex);

        if (m_FreeSamplerIndices.empty()) {
            LOG_ERROR("Bindless Sampler heap ran out of indices");
            return 0;
        }

        u32 index = m_FreeSamplerIndices.front();
        m_FreeSamplerIndices.pop();

        VkDescriptorImageInfo imageInfo {
            .sampler = sampler,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_Set,
            .dstBinding = 1,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);

        return index;
    }

    void DescriptorManager::UnregisterSampler(u32 index)
    {
        std::scoped_lock lock(m_SamplerMutex);
        m_FreeSamplerIndices.push(index);
    }

    void DescriptorManager::Bind(VkCommandBuffer cmd, VkPipelineBindPoint bind, VkPipelineLayout layout, u32 index)
    {
        vkCmdBindDescriptorSets(cmd, bind, layout, index, 1, &m_Set, 0, nullptr);
    }

    void DescriptorManager::UpdateStorageImage(u32 binding, VkImageView view, VkImageLayout layout)
    {
        VkDescriptorImageInfo imageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = view,
            .imageLayout = layout
        };

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_Set,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);
    }

    void DescriptorManager::UpdateTLAS(u32 binding, VkAccelerationStructureKHR tlas)
    {
        VkWriteDescriptorSetAccelerationStructureKHR asInfo {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas
        };

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &asInfo,
            .dstSet = m_Set,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);
    }

    void DescriptorManager::InitLayout()
    {
        m_Layout = DescriptorLayoutBuilder(m_Device)
            .AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, s_MaxBindlessTextures, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLER, s_MaxBindlessSamplers, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(3, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
            .Build();
    }

    void DescriptorManager::InitPool()
    {
        std::vector<VkDescriptorPoolSize> sizes = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, s_MaxBindlessTextures },
            { VK_DESCRIPTOR_TYPE_SAMPLER, s_MaxBindlessSamplers },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 }
        };

        VkDescriptorPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = 1,
            .poolSizeCount = static_cast<u32>(sizes.size()),
            .pPoolSizes = sizes.data()
        };

        VK_CHECK(vkCreateDescriptorPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_Pool));

        VkDescriptorSetAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_Pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_Layout
        };

        VK_CHECK(vkAllocateDescriptorSets(m_Device->GetDevice(), &allocateInfo, &m_Set));
    }

}
