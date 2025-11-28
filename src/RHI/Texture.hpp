#pragma once

#include "VkTypes.hpp"
#include "Image.hpp"
#include "Sampler.hpp"

namespace RHI {

    class Device;

    class Texture
    {
    public:
        Texture(const std::shared_ptr<Device>& device, const Image::Spec& image, const Sampler::Spec& sampler);
        Texture(const std::shared_ptr<Image>& image, const std::shared_ptr<Sampler>& sampler);

        ~Texture() = default;

        inline std::shared_ptr<Image> GetImage() const { return m_Image; }
        inline std::shared_ptr<Sampler> GetSampler() const { return m_Sampler; }

        inline u32 GetImageIndex() const { return m_ImageIndex; }
        inline u32 GetSamplerIndex() const { return m_SamplerIndex; }

        inline void SetBindlessIndices(u32 image, u32 sampler)
        {
            m_ImageIndex = image;
            m_SamplerIndex = sampler;
        }

    private:
        std::shared_ptr<Image> m_Image;
        std::shared_ptr<Sampler> m_Sampler;

        u32 m_ImageIndex { 0 };
        u32 m_SamplerIndex { 0 };
    };

}
