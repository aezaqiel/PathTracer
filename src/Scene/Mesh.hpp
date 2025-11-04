#pragma once

#include <glm/glm.hpp>

#include "Core/Types.hpp"

namespace PathTracer {

    struct Vertex
    {
        glm::vec3 position { 0.0f, 0.0f, 0.0f };
        glm::vec3 normal { 0.0f, 1.0f, 0.0f };
        glm::vec2 texCoord { 0.0f, 0.0f };
    };

    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        u32 materialIndex { 0 };
    };

}
