#pragma once

#include "Mesh.hpp"

namespace PathTracer {

    struct Material
    {
        alignas(16) glm::vec3 color;
    };

    class Model
    {
    public:
        Model(const std::string& filename);
        ~Model() = default;

        const std::vector<Mesh>& GetMeshes() const { return m_Meshes; }
        const std::vector<Material>& GetMaterials() const { return m_Materials; }

    private:
        void LoadGLTF(const std::string& filepath);

        std::vector<Mesh> m_Meshes;
        std::vector<Material> m_Materials;
    };

}
