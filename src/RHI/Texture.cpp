#include "Texture.hpp"

namespace RHI {

    Texture::Texture(const std::shared_ptr<Device>& device, const Image::Spec& image, const Sampler::Spec& sampler)
    {
        m_Image = std::make_shared<Image>(device, image);
        m_Sampler = std::make_shared<Sampler>(device, sampler);
    }

    Texture::Texture(const std::shared_ptr<Image>& image, const std::shared_ptr<Sampler>& sampler)
        : m_Image(image), m_Sampler(sampler)
    {
    }

}
