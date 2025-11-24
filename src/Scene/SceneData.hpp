#pragma once

#include <glm/glm.hpp>

#include "Types.hpp"

namespace Scene {

    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv0;
        glm::vec4 tangent;
    };

    struct ImageData
    {
        u32 width = 0;
        u32 height = 0;
        u32 channels = 4;
        std::vector<std::byte> pixels;
    };

    struct MaterialData
    {
        enum class AlphaMode : i32
        {
            Opaque,
            Mask,
            Blend
        };

        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        glm::vec3 emissiveFactor = glm::vec4(0.0f);

        f32 metallicFactor = 1.0f;
        f32 roughnessFactor = 1.0f;

        f32 alphaCutoff = 0.5f;
        AlphaMode alphaMode = AlphaMode::Opaque;

        i32 baseColorTexture = -1;
        i32 metallicRoughnessTexture = -1;
        i32 normalTexture = -1;
        i32 occlusionTexture = -1;
        i32 emissiveTexture = -1;

        i32 _p0[1];
    };

    struct MeshPrimitive
    {
        u32 indexOffset = 0;
        u32 indexCount = 0;
        u32 vertexOffset = 0;
        u32 materialIndex = 0;
    };

    struct RenderObject
    {
        glm::mat4 transform = glm::mat4(1.0f);
        u32 primitiveIndex;
    };

    struct SceneData
    {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        std::vector<MaterialData> materials;
        std::vector<ImageData> textures;

        std::vector<MeshPrimitive> primitives;
        std::vector<RenderObject> renderables;
    };

}
