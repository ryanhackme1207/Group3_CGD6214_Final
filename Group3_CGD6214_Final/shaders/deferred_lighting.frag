#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 viewPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

void main()
{             
    // Retrieve data from G-buffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec4 AlbedoSpec = texture(gAlbedoSpec, TexCoords);
    vec3 Diffuse = AlbedoSpec.rgb;
    float Specular = AlbedoSpec.a;
    
    // Calculate lighting
    vec3 ambient = ambientColor * Diffuse;
    
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(-lightDir + viewDir);
    
    float diff = max(dot(Normal, -lightDir), 0.0);
    vec3 diffuse = diff * lightColor * Diffuse;
    
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 32.0);
    vec3 specular = spec * Specular * lightColor;
    
    // Allow values to go beyond 1.0 for HDR
    vec3 result = ambient + diffuse + specular;
    
    // Output HDR values directly, tone mapping will be applied later
    FragColor = vec4(result, 1.0);
}
