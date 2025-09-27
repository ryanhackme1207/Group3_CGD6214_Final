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

// Point light properties
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

// Spot light properties
struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float innerCutoff; // cos(angle)
    float outerCutoff; // cos(angle)
    float constant;
    float linear;
    float quadratic;
};

uniform Material material;
uniform PointLight pointLights[64];
uniform int numPointLights;
uniform SpotLight spotLights[16];
uniform int numSpotLights;
uniform vec3 ambientColor; // global ambient

// Simple lighting uniforms (for backward compatibility)
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform vec3 objectColor;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Start with global ambient
    vec3 result = ambientColor * objectColor * 0.15;

    // If simple single light is provided, use it as a directional-like source
    if (length(lightColor) > 0.001) {
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        float specularStrength = 0.6;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
        vec3 specular = specularStrength * spec * lightColor;

        result += (diffuse + specular) * objectColor;
    }

    // Add contributions from point lights
    for (int i = 0; i < numPointLights; ++i) {
        PointLight pl = pointLights[i];
        vec3 lightDir = normalize(pl.position - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        // Specular
        float specularStrength = 0.6;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
        vec3 specular = specularStrength * spec * pl.color * pl.intensity;
        // Attenuation
        float distance = length(pl.position - FragPos);
        float attenuation = 1.0 / (pl.constant + pl.linear * distance + pl.quadratic * distance * distance);
        vec3 diffuse = diff * pl.color * pl.intensity;
        diffuse *= attenuation;
        specular *= attenuation;
        // Ambient from this point light (very small)
        vec3 ambient = 0.05 * pl.color * pl.intensity * attenuation;
        result += (ambient + diffuse + specular) * objectColor;
    }

    // Add contributions from spotlights
    for (int i = 0; i < numSpotLights; ++i) {
        SpotLight sl = spotLights[i];
        vec3 lightDir = normalize(sl.position - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);

        // Attenuation
        float distance = length(sl.position - FragPos);
        float attenuation = 1.0 / (sl.constant + sl.linear * distance + sl.quadratic * distance * distance);

        // Spotlight intensity (soft edges)
        vec3 spotDir = normalize(-sl.direction); // direction the spotlight points toward
        float theta = dot(lightDir, spotDir);
        float epsilon = sl.innerCutoff - sl.outerCutoff;
        float intensity = clamp((theta - sl.outerCutoff) / max(epsilon, 0.001), 0.0, 1.0);

        // Specular
        float specularStrength = 0.6;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
        vec3 specular = specularStrength * spec * sl.color * sl.intensity * intensity;

        vec3 diffuse = diff * sl.color * sl.intensity * intensity;
        diffuse *= attenuation;
        specular *= attenuation;
        vec3 ambient = 0.03 * sl.color * sl.intensity * attenuation;

        result += (ambient + diffuse + specular) * objectColor;
    }

    // Slight fog for distance
    float distanceToCamera = length(viewPos - FragPos);
    float fogFactor = exp(-pow(distanceToCamera / 200.0, 2.0));
    vec3 finalColor = mix(vec3(0.05, 0.05, 0.08), result, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}