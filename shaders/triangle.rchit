#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT vec3 payload;
hitAttributeEXT vec2 barycentrics;

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texCoord;
};

struct Material
{
    vec3 baseColorFactor;
    float _p0;
    vec3 emissiveFactor;
    float _p1;
    float metallicFactor;
    float roughnessFactor;
    int baseColorTexture;
    int metallicRoughnessTexture;
    int normalTexture;
    int emissiveTexture;
    int occlusionTexture;
};

layout(binding = 3, set = 0, std430) buffer Materials {
    Material materials[];
} u_Materials;

layout(binding = 4, set = 0) uniform sampler2D u_Textures[];

layout(binding = 5, set = 0, std430) buffer readonly Vertices
{
    Vertex vertices[];
} u_Vertices;

layout(binding = 6, set = 0, std430) buffer readonly Indices
{
    uint indices[];
} u_Indices;

void main()
{
    Material mat = u_Materials.materials[gl_InstanceCustomIndexEXT];

    uint primID = gl_PrimitiveID;

    uint i0 = u_Indices.indices[primID * 3 + 0];
    uint i1 = u_Indices.indices[primID * 3 + 1];
    uint i2 = u_Indices.indices[primID * 3 + 2];

    Vertex v0 = u_Vertices.vertices[i0];
    Vertex v1 = u_Vertices.vertices[i1];
    Vertex v2 = u_Vertices.vertices[i2];

    const float b0 = 1.0 - barycentrics.x - barycentrics.y;
    const float b1 = barycentrics.x;
    const float b2 = barycentrics.y;

    vec2 texCoord = v0.texCoord * b0 + v1.texCoord * b1 + v2.texCoord * b2;

    vec3 baseColor = mat.baseColorFactor;
    if (mat.baseColorTexture > -1) {
        baseColor *= texture(u_Textures[nonuniformEXT(mat.baseColorTexture)], texCoord).rgb;
    }

    vec3 emissive = mat.emissiveFactor;
    if (mat.emissiveTexture > -1) {
        emissive *= texture(u_Textures[nonuniformEXT(mat.emissiveTexture)], texCoord).rgb;
    }

    vec3 finalColor = baseColor + emissive;

    payload = finalColor;
}
