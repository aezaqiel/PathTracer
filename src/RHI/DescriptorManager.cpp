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

    DescriptorAllocator::DescriptorAllocator(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    DescriptorAllocator::~DescriptorAllocator()
    {
        if (m_CurrentPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_Device->GetDevice(), m_CurrentPool, nullptr);
        }

        for (auto pool : m_UsedPools) {
            vkDestroyDescriptorPool(m_Device->GetDevice(), pool, nullptr);
        }

        for (auto pool : m_FreePools) {
            vkDestroyDescriptorPool(m_Device->GetDevice(), pool, nullptr);
        }
    }

    bool DescriptorAllocator::Allocate(VkDescriptorSetLayout layout, VkDescriptorSet& set)
    {
        if (m_CurrentPool == VK_NULL_HANDLE) {
            m_CurrentPool = GrabPool();
            m_UsedPools.push_back(m_CurrentPool);
        }

        VkDescriptorSetAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_CurrentPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout
        };

        VkResult result = vkAllocateDescriptorSets(m_Device->GetDevice(), &allocateInfo, &set);

        switch (result) {
            case VK_SUCCESS: return true;
            case VK_ERROR_FRAGMENTED_POOL:
            case VK_ERROR_OUT_OF_POOL_MEMORY:
                break;
            default:
                return false;
        }

        m_CurrentPool = GrabPool();
        m_UsedPools.push_back(m_CurrentPool);

        allocateInfo.descriptorPool = m_CurrentPool;
        VK_CHECK(vkAllocateDescriptorSets(m_Device->GetDevice(), &allocateInfo, &set));

        return true;
    }

    void DescriptorAllocator::Reset()
    {
        for (auto pool : m_UsedPools) {
            vkResetDescriptorPool(m_Device->GetDevice(), pool, 0);
            m_FreePools.push_back(pool);
        }

        m_UsedPools.clear();
        m_CurrentPool = VK_NULL_HANDLE;
    }

    VkDescriptorPool DescriptorAllocator::GrabPool()
    {
        if (!m_FreePools.empty()) {
            VkDescriptorPool pool = m_FreePools.back();
            m_FreePools.pop_back();
            return pool;
        }

        std::vector<VkDescriptorPoolSize> sizes = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 500 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 500 }
        };

        VkDescriptorPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = 1000,
            .poolSizeCount = static_cast<u32>(sizes.size()),
            .pPoolSizes = sizes.data()
        };

        VkDescriptorPool pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(m_Device->GetDevice(), &poolInfo, nullptr, &pool));

        return pool;
    }

    DescriptorWriter::DescriptorWriter(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    DescriptorWriter& DescriptorWriter::WriteImage(u32 binding, VkImageView view, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
    {
        VkDescriptorImageInfo& info = m_ImageInfos.emplace_back(VkDescriptorImageInfo {
            .sampler = sampler,
            .imageView = view,
            .imageLayout = layout
        });

        m_Writes.push_back(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VK_NULL_HANDLE,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = &info,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    DescriptorWriter& DescriptorWriter::WriteBuffer(u32 binding, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset, VkDescriptorType type)
    {
        VkDescriptorBufferInfo& info = m_BufferInfos.emplace_back(VkDescriptorBufferInfo {
            .buffer = buffer,
            .offset = offset,
            .range = size
        });

        m_Writes.push_back(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VK_NULL_HANDLE,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = nullptr,
            .pBufferInfo = &info,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    DescriptorWriter& DescriptorWriter::WriteAS(u32 binding, VkAccelerationStructureKHR as)
    {
        VkAccelerationStructureKHR& pAS = m_ASes.emplace_back(as);

        VkWriteDescriptorSetAccelerationStructureKHR& info = m_ASInfos.emplace_back(VkWriteDescriptorSetAccelerationStructureKHR {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &pAS
        });

        m_Writes.push_back(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &info,
            .dstSet = VK_NULL_HANDLE,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    bool DescriptorWriter::Build(VkDescriptorSet& set, VkDescriptorSetLayout layout, DescriptorAllocator& allocator)
    {
        if (!allocator.Allocate(layout, set)) {
            return false;
        }

        Overwrite(set);
        return true;
    }

    void DescriptorWriter::Overwrite(VkDescriptorSet set)
    {
        for (auto& write : m_Writes) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(m_Device->GetDevice(), static_cast<u32>(m_Writes.size()), m_Writes.data(), 0, nullptr);
    }

    DescriptorManager::DescriptorManager(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
        m_Allocator = std::make_unique<DescriptorAllocator>(m_Device);

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
