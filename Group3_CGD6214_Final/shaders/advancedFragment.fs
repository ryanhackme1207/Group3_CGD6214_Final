#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;
in vec3 TangentLightPos;
in vec3 TangentViewPos;
in vec3 TangentFragPos;

// Material properties
struct Material {
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
    sampler2D emissionMap;

    // PBR properties
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    vec3 emission;
    float emissionStrength;

    // Phong properties
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    float reflectivity;
    float transparency;

    // Texture flags
    bool hasDiffuseMap;
    bool hasSpecularMap;
    bool hasNormalMap;
    bool hasEmissionMap;

    // Animation
    vec2 textureOffset;
};

// Light structure
struct Light {
    int type; // 0: directional, 1: point, 2: spot
    vec3 position;
    vec3 direction;
    vec3 color;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    
    float constant;
    float linear;
    float quadratic;
    
    float cutOff;
    float outerCutOff;
    float intensity;
};

uniform Material material;
uniform Light lights[8];
uniform int numLights;
uniform vec3 viewPos;
uniform vec3 globalAmbient;
uniform sampler2D shadowMap;
uniform bool enableShadows;
uniform samplerCube skybox;
uniform bool enableReflections;

// Function prototypes
vec3 CalcDirLight(Light light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor);
vec3 CalcPointLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor);
vec3 CalcSpotLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor);
float ShadowCalculation(vec4 fragPosLightSpace);
vec3 getNormalFromMap();

void main() {
    // Sample material textures or use material properties
    vec2 texCoord = TexCoord + material.textureOffset;

    vec3 diffuseColor;
    vec3 specularColor;

    // Use texture if available, otherwise use material color
    if (material.hasDiffuseMap) {
        diffuseColor = texture(material.diffuseMap, texCoord).rgb;
    } else {
        // Use material diffuse color, fallback to albedo if diffuse is black
        diffuseColor = length(material.diffuse) > 0.0 ? material.diffuse : material.albedo;
    }

    if (material.hasSpecularMap) {
        specularColor = texture(material.specularMap, texCoord).rgb;
    } else {
        specularColor = material.specular;
    }

    // Ensure we have some color
    if (length(diffuseColor) == 0.0) {
        diffuseColor = vec3(0.7, 0.7, 0.7); // Default gray
    }

    // Get normal (either from normal map or vertex normal)
    vec3 norm;
    if (material.hasNormalMap) {
        norm = getNormalFromMap();
    } else {
        norm = normalize(Normal);
    }

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 result = globalAmbient * diffuseColor;
    
    // Calculate lighting for each light source
    for (int i = 0; i < numLights && i < 8; i++) {
        if (lights[i].type == 0) {
            // Directional light
            result += CalcDirLight(lights[i], norm, viewDir, diffuseColor, specularColor);
        } else if (lights[i].type == 1) {
            // Point light
            result += CalcPointLight(lights[i], norm, FragPos, viewDir, diffuseColor, specularColor);
        } else if (lights[i].type == 2) {
            // Spot light
            result += CalcSpotLight(lights[i], norm, FragPos, viewDir, diffuseColor, specularColor);
        }
    }
    
    // Add emission if available
    if (material.hasEmissionMap) {
        vec3 emission = texture(material.emissionMap, texCoord).rgb * material.emissionStrength;
        result += emission;
    } else if (length(material.emission) > 0.0) {
        result += material.emission * material.emissionStrength;
    }
    
    // Add reflections if enabled
    if (enableReflections) {
        vec3 I = normalize(FragPos - viewPos);
        vec3 R = reflect(I, norm);
        vec3 reflection = texture(skybox, R).rgb;
        result = mix(result, reflection, material.reflectivity);
    }
    
    // Apply HDR tone mapping and gamma correction
    result = result / (result + vec3(1.0));
    result = pow(result, vec3(1.0/2.2));
    
    FragColor = vec4(result, 1.0);
}

// Directional light calculation
vec3 CalcDirLight(Light light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    
    // Shadow calculation
    float shadow = enableShadows ? ShadowCalculation(FragPosLightSpace) : 0.0;
    
    // Combine results
    vec3 ambient = light.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor * light.intensity;
    vec3 specular = light.specular * spec * specularColor * light.intensity;
    
    return ambient + (1.0 - shadow) * (diffuse + specular);
}

// Point light calculation
vec3 CalcPointLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Combine results
    vec3 ambient = light.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;
    
    return (ambient + diffuse + specular) * attenuation * light.intensity;
}

// Spot light calculation
vec3 CalcSpotLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    // Combine results
    vec3 ambient = light.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;
    
    return (ambient + diffuse + specular) * attenuation * intensity * light.intensity;
}

// Shadow mapping calculation
float ShadowCalculation(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0) return 0.0;
    
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    
    // Bias to prevent shadow acne
    float bias = max(0.05 * (1.0 - dot(normalize(Normal), normalize(-lights[0].direction))), 0.005);
    
    // PCF (Percentage Closer Filtering) for soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    return shadow;
}

// Normal mapping
vec3 getNormalFromMap() {
    vec2 texCoord = TexCoord + material.textureOffset;
    vec3 tangentNormal = texture(material.normalMap, texCoord).xyz * 2.0 - 1.0;
    
    vec3 Q1 = dFdx(FragPos);
    vec3 Q2 = dFdy(FragPos);
    vec2 st1 = dFdx(TexCoord);
    vec2 st2 = dFdy(TexCoord);
    
    vec3 N = normalize(Normal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * tangentNormal);
}