#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// Material properties
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

// Light properties
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    
    // Attenuation
    float constant;
    float linear;
    float quadratic;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;
uniform vec3 objectColor;

// Simple lighting uniforms (for backward compatibility)
uniform vec3 lightPos;
uniform vec3 lightColor;

void main()
{
    // Use either struct-based lighting or simple lighting
    vec3 lightPosition = length(light.position) > 0.1 ? light.position : lightPos;
    vec3 lightCol = length(light.color) > 0.1 ? light.color * light.intensity : lightColor;
    
    // Ambient
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * lightCol;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPosition - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightCol;
    
    // Specular
    float specularStrength = 0.6;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    vec3 specular = specularStrength * spec * lightCol;
    
    // Attenuation (distance-based lighting falloff)
    float distance = length(lightPosition - FragPos);
    float attenuation = 1.0 / (1.0 + 0.014 * distance + 0.0007 * (distance * distance));
    
    // Apply attenuation to diffuse and specular
    diffuse *= attenuation;
    specular *= attenuation;
    
    // Combine results
    vec3 result = (ambient + diffuse + specular) * objectColor;
    
    // Add slight fog effect for distant objects
    float fogDistance = distance / 50.0;
    float fogFactor = exp(-fogDistance * fogDistance * 0.1);
    result = mix(vec3(0.1, 0.1, 0.15), result, fogFactor);
    
    FragColor = vec4(result, 1.0);
}