#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 ambientColor = vec3(0.2);
uniform vec3 viewPos;

void main(){
    vec3 pos = texture(gPosition, TexCoord).rgb;
    vec3 normal = normalize(texture(gNormal, TexCoord).rgb);
    vec4 albedoSpec = texture(gAlbedoSpec, TexCoord);
    vec3 albedo = albedoSpec.rgb;
    float specFactor = albedoSpec.a;

    // Single hard-coded directional light for demo
    vec3 lightDir = normalize(vec3(-0.4, -1.0, -0.3));
    vec3 lightColor = vec3(1.0,0.96,0.9);

    float diff = max(dot(normal, -lightDir), 0.0);
    vec3 viewDir = normalize(viewPos - pos);
    vec3 reflectDir = reflect(lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * specFactor;

    vec3 color = ambientColor * albedo + diff * lightColor * albedo + spec * lightColor;
    FragColor = vec4(color,1.0);
}
