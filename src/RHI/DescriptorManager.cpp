#include "DescriptorManager.hpp"

#include "Device.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Sampler.hpp"
#include "Texture.hpp"
#include "AccelerationStructure.hpp"

namespace RHI {

    DescriptorLayoutBuilder::DescriptorLayoutBuilder(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
    }

    DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBinding(u32 binding, VkDescriptorType type, VkShaderStageFlags stage, u32 count)
    {
        m_Bindings.push_back(VkDescriptorSetLayoutBinding {
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = count,
            .stageFlags = stage,
            .pImmutableSamplers = nullptr
        });

        m_BindingFlags.push_back(0);

        return *this;
    }

    DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBindlessBinding(u32 binding, VkDescriptorType type, VkShaderStageFlags stage, u32 count)
    {
        m_Bindings.push_back(VkDescriptorSetLayoutBinding {
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = count,
            .stageFlags = stage,
            .pImmutableSamplers = nullptr
        });

        m_BindingFlags.push_back(
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
        );

        return *this;
    }

    VkDescriptorSetLayout DescriptorLayoutBuilder::Build()
    {
        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<u32>(m_BindingFlags.size()),
            .pBindingFlags = m_BindingFlags.data()
        };

        bool push = true;

        VkDescriptorSetLayoutCreateFlags layoutFlags = 0;
        for (auto flag : m_BindingFlags) {
            if (flag & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) {
                layoutFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                push = false;
                break;
            }
        }

        if (push) {
            layoutFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        }

        VkDescriptorSetLayoutCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &flagsInfo,
            .flags = layoutFlags,
            .bindingCount = static_cast<u32>(m_Bindings.size()),
            .pBindings = m_Bindings.data()
        };

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(m_Device->GetDevice(), &info, nullptr, &layout));

        return layout;
    }

    DescriptorAllocator::DescriptorAllocator(const std::shared_ptr<Device>& device, u32 sets, std::span<PoolSizeRatio> ratios)
        : m_Device(device), m_SetsPerPool(sets)
    {
        if (ratios.empty()) {
            m_Ratios.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f });
            m_Ratios.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f });
            m_Ratios.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f });
            m_Ratios.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.0f });
        } else {
            m_Ratios.assign(ratios.begin(), ratios.end());
        }
    }

    DescriptorAllocator::~DescriptorAllocator()
    {
        if (m_CurrentPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_Device->GetDevice(), m_CurrentPool, nullptr);
        }

        for (auto p : m_UsedPools) {
            vkDestroyDescriptorPool(m_Device->GetDevice(), p, nullptr);
        }

        for (auto p : m_FreePools) {
            vkDestroyDescriptorPool(m_Device->GetDevice(), p, nullptr);
        }
    }

    VkDescriptorSet DescriptorAllocator::Allocate(VkDescriptorSetLayout layout)
    {
        VkDescriptorPool pool = GetPool();

        VkDescriptorSetAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout
        };

        VkDescriptorSet set = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(m_Device->GetDevice(), &allocateInfo, &set);

        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
            m_UsedPools.push_back(pool);
            m_CurrentPool = VK_NULL_HANDLE;

            return Allocate(layout);
        }

        VK_CHECK(result);
        return set;
    }

    void DescriptorAllocator::Reset()
    {
        for (auto p : m_UsedPools) {
            vkResetDescriptorPool(m_Device->GetDevice(), p, 0);
            m_FreePools.push_back(p);
        }

        m_UsedPools.clear();

        if (m_CurrentPool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(m_Device->GetDevice(), m_CurrentPool, 0);
            m_FreePools.push_back(m_CurrentPool);
            m_CurrentPool = VK_NULL_HANDLE;
        }
    }

    VkDescriptorPool DescriptorAllocator::GetPool()
    {
        if (m_CurrentPool != VK_NULL_HANDLE) {
            return m_CurrentPool;
        }

        if (!m_FreePools.empty()) {
            m_CurrentPool = m_FreePools.back();
            m_FreePools.pop_back();
        } else {
            m_CurrentPool = CreatePool(m_SetsPerPool, 0);
        }

        return m_CurrentPool;
    }

    VkDescriptorPool DescriptorAllocator::CreatePool(u32 count, VkDescriptorPoolCreateFlags flags)
    {
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve(m_Ratios.size());

        for (auto ratio : m_Ratios) {
            sizes.push_back({ ratio.type, static_cast<u32>(ratio.ratio * count) });
        }

        VkDescriptorPoolCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .maxSets = count,
            .poolSizeCount = static_cast<u32>(sizes.size()),
            .pPoolSizes = sizes.data()
        };

        VkDescriptorPool pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(m_Device->GetDevice(), &info, nullptr, &pool));

        return pool;
    }

    BindlessHeap::BindlessHeap(const std::shared_ptr<Device>& device)
        : m_Device(device)
    {
        m_Layout = DescriptorLayoutBuilder(m_Device)
            .AddBindlessBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL, MAX_BINDLESS_RESOURCES)
            .Build();

        VkDescriptorPoolSize poolSize {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_BINDLESS_RESOURCES
        };

        VkDescriptorPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
        };

        VK_CHECK(vkCreateDescriptorPool(device->GetDevice(), &poolInfo, nullptr, &m_Pool));

        u32 maxBinding = MAX_BINDLESS_RESOURCES - 1;
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &maxBinding
        };

        VkDescriptorSetAllocateInfo allocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variableInfo,
            .descriptorPool = m_Pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_Layout
        };

        VK_CHECK(vkAllocateDescriptorSets(device->GetDevice(), &allocateInfo, &m_Set));

        for (u32 i = 0; i < MAX_BINDLESS_RESOURCES; ++i) {
            m_FreeIndices.push(i);
        }
    }

    BindlessHeap::~BindlessHeap()
    {
        vkDestroyDescriptorPool(m_Device->GetDevice(), m_Pool, nullptr);
        vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_Layout, nullptr);
    }

    u32 BindlessHeap::RegisterTexture(const Texture& texture)
    {
        u32 index = AllocateIndex();
        if (index == std::numeric_limits<u32>::max()) return 0;

        UpdateTexture(index, texture);
        return index;
    }

    void BindlessHeap::UnregisterTexture(u32 index)
    {
        FreeIndex(index);
    }

    void BindlessHeap::Bind(VkCommandBuffer cmd, VkPipelineBindPoint bind, VkPipelineLayout layout)
    {
        vkCmdBindDescriptorSets(cmd, bind, layout, 0, 1, &m_Set, 0, nullptr);
    }

    u32 BindlessHeap::AllocateIndex()
    {
        std::scoped_lock<std::mutex> lock(m_AllocationMutex);

        if (m_FreeIndices.empty()) {
            LOG_ERROR("Bindless heap full");
            return std::numeric_limits<u32>::max();
        }

        u32 index = m_FreeIndices.front();
        m_FreeIndices.pop();

        return index;
    }

    void BindlessHeap::FreeIndex(u32 index)
    {
        std::scoped_lock<std::mutex> lock(m_AllocationMutex);
        m_FreeIndices.push(index);
    }

    void BindlessHeap::UpdateTexture(u32 index, const Texture& texture)
    {
        VkDescriptorImageInfo imageInfo {
            .sampler = texture.GetSampler()->GetSampler(),
            .imageView = texture.GetImage()->GetView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_Set,
            .dstBinding = 0,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);
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

    void DescriptorWriter::Push(VkCommandBuffer cmd, VkPipelineBindPoint bind, VkPipelineLayout layout, u32 set)
    {
        vkCmdPushDescriptorSet(cmd, bind, layout, set, static_cast<u32>(m_Writes.size()), m_Writes.data());
        m_Writes.clear();
        m_ImageInfos.clear();
        m_BufferInfos.clear();
        m_ASInfos.clear();
        m_ASes.clear();
    }

}
