// Skybox Fragment Shader (skybox.fs)
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;
in vec3 WorldPos;

uniform samplerCube skybox;
uniform float dayNightCycle;
uniform vec3 sunDirection;

void main() {
    vec3 color = texture(skybox, TexCoords).rgb;

    // Simple brightness adjustment based on day/night cycle
    float dayFactor = max(0.1, max(0.0, sunDirection.y) * 0.8 + 0.2);
    color *= dayFactor;

    FragColor = vec4(color, 1.0);
}