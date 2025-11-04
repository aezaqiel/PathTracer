#pragma once

#include "Texture.hpp"
#include "Model.hpp"

namespace PathTracer {

    class AssetManager
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        std::shared_ptr<Texture> GetTexture(const std::string& filename);
        std::shared_ptr<Model> GetModel(const std::string& filename);

    private:
        std::shared_ptr<Texture> LoadTextureFromFile(const std::string& filepath);
        std::shared_ptr<Model> LoadModelFromFile(const std::string& filepath);

        std::string GetFullAssetPath(const std::string& filename);

        std::map<std::string, std::shared_ptr<Texture>> m_TextureCache;
        std::map<std::string, std::shared_ptr<Model>> m_ModelCache;
    };

}
