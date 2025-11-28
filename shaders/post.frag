#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler samps[];

layout(push_constant) uniform PushConsts {
    uint textureID;
    uint samplerID;
} pc;

void main() 
{
    // vec3 color = texture(textures[nonuniformEXT(pc.textureID)], inUV).rgb;
    vec3 color = texture(sampler2D(textures[pc.textureID], samps[pc.samplerID]), inUV).rgb;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
