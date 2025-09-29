#version 330 core
layout (location=0) out vec3 gPosition;
layout (location=1) out vec3 gNormal;
layout (location=2) out vec4 gAlbedoSpec;

in VS_OUT { vec3 FragPos; vec3 Normal; vec2 Tex; } fs_in;

uniform vec3 objectColor = vec3(1.0);
uniform float specularStrength = 0.5;

void main(){
    gPosition = fs_in.FragPos;
    gNormal = normalize(fs_in.Normal);
    gAlbedoSpec.rgb = objectColor;
    gAlbedoSpec.a = specularStrength; // store specular factor in alpha
}
