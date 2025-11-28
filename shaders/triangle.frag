#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushContants {
    uint outputTextureID;
} pc;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1280.0, 720.0);
    vec3 color = texture(textures[pc.outputTextureID], uv).rgb;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
