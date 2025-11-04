#pragma once

#include <glm/glm.hpp>

#include "Core/Types.hpp"

namespace PathTracer {

    struct Vertex
    {
        glm::vec3 position;
    };

    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        u32 materialIndex { 0 };
    };

}
