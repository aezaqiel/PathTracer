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

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

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

    vec3 albedo = mat.baseColorFactor.rgb;
    if (mat.baseColorTexture >= 0) {
        vec3 texColor = texture(g_Textures[nonuniformEXT(mat.baseColorTexture)], uv).rgb;
        albedo *= pow(texColor, vec3(2.2));
    }

    float metallic = mat.metallicFactor;
    float roughness = mat.roughnessFactor;

    if (mat.metallicRoughnessTexture >= 0) {
        vec4 mrSample = texture(g_Textures[nonuniformEXT(mat.metallicRoughnessTexture)], uv);
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }

    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    vec3 L = normalize(vec3(0.5, 1.0, 0.2));
    vec3 H = normalize(V + L);

    vec3 lightColor = vec3(3.0);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0 - kS);
    kD *= 1.0 - metallic;

    float NdotL = max(dot(normal, L), 0.0);

    vec3 Lo = (kD * albedo / PI + specular) * lightColor * NdotL;

    vec3 emissive = mat.emissiveFactor;
    if (mat.emissiveTexture >= 0) {
        emissive *= texture(g_Textures[nonuniformEXT(mat.emissiveTexture)], uv).rgb;
    }

    payload = Lo + emissive;
}
