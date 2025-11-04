#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

struct Material {
    vec3 color;
    float padding;
};

layout(binding = 3, set = 0) buffer Materials {
    Material materials[];
} u_Materials;

void main()
{
    Material mat = u_Materials.materials[gl_InstanceCustomIndexEXT];
    payload = mat.color;
}
