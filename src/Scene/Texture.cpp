#include "Texture.hpp"

#include <stb_image.h>

#include "Core/Logger.hpp"

namespace PathTracer {

    Texture::Texture(const std::string& filepath)
    {
        m_Data = stbi_load(filepath.c_str(), &m_Width, &m_Height, &m_Channels, 4);
        if (!m_Data) {
            LOG_ERROR("Failed to load texture file: {}", filepath);
        } else {
            LOG_INFO("Loaded texture: {}", filepath);
        }
    }

    Texture::~Texture()
    {
        if (m_Data) {
            stbi_image_free(m_Data);
        }
    }

}
