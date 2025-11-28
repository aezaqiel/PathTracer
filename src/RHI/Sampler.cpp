#include "Sampler.hpp"

#include "Device.hpp"

namespace RHI {

    Sampler::Sampler(const std::shared_ptr<Device>& device, const Spec& spec)
        : m_Device(device)
    {
        VkSamplerCreateInfo samplerInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = spec.magFilter,
            .minFilter = spec.minFilter,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = spec.addressModeU,
            .addressModeV = spec.addressModeV,
            .addressModeW = spec.addressModeW,
            .mipLodBias = 0.0f,
            .anisotropyEnable = (spec.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = spec.maxAnisotropy,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = VK_LOD_CLAMP_NONE,
            .borderColor = spec.borderColor,
            .unnormalizedCoordinates = VK_FALSE
        };

        VK_CHECK(vkCreateSampler(device->GetDevice(), &samplerInfo, nullptr, &m_Sampler));
    }

    Sampler::~Sampler()
    {
        vkDestroySampler(m_Device->GetDevice(), m_Sampler, nullptr);
    }

}
