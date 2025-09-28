#version 330 core
// Fragment shader implementing normal mapping (tangent-space normal map)
// Expects the vertex shader to provide: FragPos, TexCoord, TBN matrix (as mat3)

in vec3 FragPos;
in vec2 TexCoord;
in mat3 TBN; // tangent to world space matrix - computed in vertex shader

out vec4 FragColor;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float shininess;
uniform vec3 ambientColor;

void main()
{
    // Sample normal from normal map (in range [0,1]) and remap to [-1,1]
    vec3 n = texture(normalMap, TexCoord).rgb;
    n = normalize(n * 2.0 - 1.0);

    // Transform normal from tangent space to world space
    vec3 N = normalize(TBN * n);

    // Material diffuse color
    vec3 albedo = texture(diffuseMap, TexCoord).rgb;

    // Ambient term
    vec3 ambient = 0.15 * ambientColor * albedo;

    // Diffuse term
    vec3 L = normalize(lightPos - FragPos);
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * lightColor * albedo;

    // Specular term (Blinn-Phong)
    vec3 V = normalize(viewPos - FragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), max(1.0, shininess));
    vec3 specular = spec * lightColor;

    vec3 color = ambient + diffuse + specular;
    FragColor = vec4(color, 1.0);
}
