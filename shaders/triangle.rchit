#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

struct Material {
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

void main()
{
    Material mat = u_Materials.materials[gl_InstanceCustomIndexEXT];
    
    vec3 finalColor = mat.baseColorFactor;
    
    finalColor += mat.emissiveFactor;

    payload = finalColor;
}
