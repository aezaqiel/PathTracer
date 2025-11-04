#pragma once

#include "Mesh.hpp"
#include "Texture.hpp"

namespace PathTracer {

    struct Material
    {
        alignas(16) glm::vec3 baseColorFactor { 1.0f };
        alignas(16) glm::vec3 emissiveFactor { 0.0f };

        f32 metallicFactor { 1.0f };
        f32 roughnessFactor { 1.0f };

        i32 baseColorTexture { -1 };
        i32 metallicRoughnessTexture { -1 };
        i32 normalTexture { -1 };
        i32 emissiveTexture { -1 };
        i32 occlusionTexture { -1 };
    };

    struct Model
    {
        Model() = default;
        ~Model() = default;

        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<std::shared_ptr<Texture>> textures;

        inline const usize GetMeshCount() const { return meshes.size(); }
        inline const usize GetMaterialCount() const { return materials.size(); }
        inline const usize GetTextureCount() const { return textures.size(); }
    };

}
