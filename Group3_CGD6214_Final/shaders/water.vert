#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

out vec3 WorldPos;
out vec2 TexCoord;
out vec3 Normal;
out vec2 DudvUV;

void main() {
    // base position in model space
    vec3 pos = aPos;

    // Multiple wave layers (Gerstner-like approximation)
    float k1 = 8.0; float k2 = 12.0; float k3 = 18.0; // wave frequencies
    float a1 = 0.06; float a2 = 0.035; float a3 = 0.02; // amplitudes
    float s1 = 1.0; float s2 = 1.6; float s3 = 0.8; // speeds

    // use texture coordinates to drive wave patterns (tiling expected)
    float w1 = sin(aTexCoord.x * k1 + time * s1) * a1;
    float w2 = cos(aTexCoord.y * k2 - time * s2) * a2;
    float w3 = sin((aTexCoord.x + aTexCoord.y) * k3 + time * s3) * a3;
    float height = w1 + w2 + w3;
    pos.y += height;

    // analytic normal approximation from partial derivatives
    float dhdx = cos(aTexCoord.x * k1 + time * s1) * (k1 * a1)
               + cos((aTexCoord.x + aTexCoord.y) * k3 + time * s3) * (k3 * a3);
    float dhdz = -sin(aTexCoord.y * k2 - time * s2) * (k2 * a2)
               + cos((aTexCoord.x + aTexCoord.y) * k3 + time * s3) * (k3 * a3);
    vec3 n = normalize(vec3(-dhdx, 1.0, -dhdz));

    vec4 world = model * vec4(pos, 1.0);
    WorldPos = world.xyz;
    TexCoord = aTexCoord;
    Normal = mat3(transpose(inverse(model))) * n; // transform normal to world space

    // dudv UV for fragment distortion (animated)
    DudvUV = aTexCoord + vec2(time * 0.02, time * 0.03);

    gl_Position = projection * view * world;
}
