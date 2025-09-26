// Skybox Fragment Shader (skybox.fs)
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform vec3 skyTint;
uniform float skyIntensity;

void main() {
    vec3 color = texture(skybox, TexCoords).rgb;
    color *= skyTint * skyIntensity;
    FragColor = vec4(color, 1.0);
}