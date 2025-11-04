#pragma once

#include "Core/Types.hpp"

namespace PathTracer {

    class Texture
    {
    public:
        Texture(const std::string& filepath);
        ~Texture();

        inline u8* GetData() const { return m_Data; }
        inline i32 GetWidth() const { return m_Width; }
        inline i32 GetHeight() const { return m_Height; }
        inline i32 GetChannels() const { return m_Channels; }

    private:
        u8* m_Data { nullptr };
        i32 m_Width { 0 };
        i32 m_Height { 0 };
        i32 m_Channels { 0 };
    };

}
