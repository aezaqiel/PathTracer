#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Device;

    class Sampler
    {
    public:
        struct Spec
        {
            VkFilter magFilter { VK_FILTER_LINEAR };
            VkFilter minFilter { VK_FILTER_LINEAR };
            VkSamplerAddressMode addressModeU { VK_SAMPLER_ADDRESS_MODE_REPEAT };
            VkSamplerAddressMode addressModeV { VK_SAMPLER_ADDRESS_MODE_REPEAT };
            VkSamplerAddressMode addressModeW { VK_SAMPLER_ADDRESS_MODE_REPEAT };
            f32 maxAnisotropy { 1.0f };
            VkBorderColor borderColor { VK_BORDER_COLOR_INT_OPAQUE_BLACK };
        };

    public:
        Sampler(const std::shared_ptr<Device>& device, const Spec& spec);
        ~Sampler();

        inline VkSampler GetSampler() const { return m_Sampler; }

    private:
        std::shared_ptr<Device> m_Device;

        VkSampler m_Sampler { VK_NULL_HANDLE };
    };

}
