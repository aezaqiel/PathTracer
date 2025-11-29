#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(location = 0) rayPayloadInEXT vec3 payload;
hitAttributeEXT vec2 attribs;

layout(set = 0, binding = 0) uniform sampler2D g_Textures[];

layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;

struct RenderObject
{
    uint64_t vertex;
    uint64_t index;
    uint material;
    uint padding;
};

layout(set = 1, binding = 3, scalar) buffer ObjDesc
{
    RenderObject objects[];
} objs;

struct Material
{
    vec4 baseColorFactor;
    vec3 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    int alphaMode;
    int baseColorTexture;
    int metallicRoughnessTexture;
    int normalTexture;
    int occlusionTexture;
    int emissiveTexture;
    int padding[3];
};

layout(set = 1, binding = 4, scalar) buffer Materials
{
    Material mat[];
} materials;

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
};

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uint i[]; };

void main()
{
    uint objID = gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT;
    RenderObject obj = objs.objects[objID];

    Vertices vertices = Vertices(obj.vertex);
    Indices indices = Indices(obj.index);

    uint ind0, ind1, ind2;
    if (obj.index != 0) {
        ind0 = indices.i[gl_PrimitiveID * 3 + 0];
        ind1 = indices.i[gl_PrimitiveID * 3 + 1];
        ind2 = indices.i[gl_PrimitiveID * 3 + 2];
    } else {
        ind0 = gl_PrimitiveID * 3 + 0;
        ind1 = gl_PrimitiveID * 3 + 1;
        ind2 = gl_PrimitiveID * 3 + 2;
    }

    Vertex v0 = vertices.v[ind0];
    Vertex v1 = vertices.v[ind1];
    Vertex v2 = vertices.v[ind2];

    const vec3 barycentric = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    
    vec3 normal = v0.normal * barycentric.x + v1.normal * barycentric.y + v2.normal + barycentric.z;
    vec2 uv = v0.uv * barycentric.x + v1.uv * barycentric.y + v2.uv * barycentric.z;

    normal = normalize(vec3(gl_ObjectToWorldEXT * vec4(normal, 0.0)));

    Material mat = materials.mat[obj.material];
    vec3 color = mat.baseColorFactor.rgb;

    if (mat.baseColorTexture >= 0) {
        color *= texture(g_Textures[nonuniformEXT(mat.baseColorTexture)], uv).rgb;
    }

    vec3 sunDir = normalize(vec3(0.5, 1.0, 0.2));
    float dotNL = max(dot(normal, sunDir), 0.1);

    payload = color * dotNL;
}
