#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

in mat3 TBN; // tangent->world matrix provided by vertex shader

in vec3 TangentLightPos;
in vec3 TangentViewPos;
in vec3 TangentFragPos;

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

// Directional light (optional)
uniform vec3 lightDir; // direction of light rays

// Extra bump/global control
uniform float bumpIntensity;

uniform bool isGround;
uniform float groundBumpIntensity;
uniform float time;

uniform sampler2D diffuseTex;
uniform bool hasTexture;

// Normal mapping
uniform sampler2D normalMap;
uniform bool useNormalMap;

// Shadow map for the primary spotlight (index 0)
uniform sampler2D spotShadowMap;
uniform mat4 spotLightSpace;

float groundNoise(vec2 coord) {
    float noise = 0.0;
    noise += sin(coord.x * 5.0 + time * 0.5) * 0.1;
    noise += sin(coord.y * 8.0 + time * 0.3) * 0.05;
    noise += sin((coord.x + coord.y) * 12.0 + time * 0.7) * 0.03;
    return noise;
}

vec3 groundBumpNormal(vec2 texCoord) {
    vec2 groundCoord = texCoord * 20.0;
    
    float d = 0.01;
    
    float h = groundNoise(groundCoord);
    float hx = groundNoise(groundCoord + vec2(d, 0.0));
    float hy = groundNoise(groundCoord + vec2(0.0, d));

    vec3 normal = normalize(vec3(h - hx, 0.1, h - hy));
    
    return normal;
}

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1]
    projCoords = projCoords * 0.5 + 0.5;
    // If outside light's frustum
    if (projCoords.z > 1.0) return 0.0;
    float bias = 0.005;
    float shadow = 0.0;
    // PCF
    float texelSize = 1.0 / 2048.0; // matches shadow map resolution used in main
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(spotShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias) > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // determine final object color, sampling diffuse texture when available
    vec3 texColor = vec3(1.0);
    if (hasTexture) {
        texColor = texture(diffuseTex, TexCoord).rgb;
    }
    vec3 objectColorFinal = texColor * objectColor;

    // combine bumpIntensity and groundBumpIntensity for effective bump
    float effectiveBump = clamp(groundBumpIntensity + bumpIntensity, 0.0, 1.0);

    // Apply normal map if requested and available (skip for ground bump which uses procedural bump)
    if (useNormalMap && !isGround) {
        vec3 n = texture(normalMap, TexCoord).rgb;
        n = normalize(n * 2.0 - 1.0);
        // transform from tangent to world
        norm = normalize(TBN * n);
    } else if (isGround && effectiveBump > 0.01) {
        vec3 bumpNorm = groundBumpNormal(TexCoord);
        norm = normalize(mix(norm, bumpNorm, effectiveBump));
    }

    // Start with global ambient
    vec3 result = ambientColor * objectColorFinal * 0.15;

    // If directional light provided, use it
    if (length(lightDir) > 0.001) {
        // lightDir is expected to be the direction the light rays travel (from light toward scene)
        vec3 L = normalize(-lightDir); // vector from fragment toward light source
        float diff = max(dot(norm, L), 0.0);
        vec3 diffuse = diff * lightColor;

        float specularStrength = 0.6;
        vec3 reflectDir = reflect(-L, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
        vec3 specular = specularStrength * spec * lightColor;

        result += (diffuse + specular) * objectColorFinal;
    }
    else if (length(lightColor) > 0.001) {
        // If simple single light is provided, use it as a point-like source
        vec3 L = normalize(lightPos - FragPos);
        float diff = max(dot(norm, L), 0.0);
        vec3 diffuse = diff * lightColor;

        float specularStrength = 0.6;
        vec3 reflectDir = reflect(-L, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
        vec3 specular = specularStrength * spec * lightColor;

        result += (diffuse + specular) * objectColorFinal;
    }

    // Add contributions from point lights
    for (int i = 0; i < numPointLights; ++i) {
        PointLight pl = pointLights[i];
        vec3 lightDirP = normalize(pl.position - FragPos);
        float diff = max(dot(norm, lightDirP), 0.0);
        // Specular
        float specularStrength = 0.6;
        vec3 reflectDir = reflect(-lightDirP, norm);
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
        result += (ambient + diffuse + specular) * objectColorFinal;
    }

    // Add contributions from spotlights
    for (int i = 0; i < numSpotLights; ++i) {
        SpotLight sl = spotLights[i];
        vec3 lightDirS = normalize(sl.position - FragPos);
        float diff = max(dot(norm, lightDirS), 0.0);

        // Attenuation
        float distance = length(sl.position - FragPos);
        float attenuation = 1.0 / (sl.constant + sl.linear * distance + sl.quadratic * distance * distance);

        // Spotlight intensity (soft edges)
        vec3 spotDir = normalize(-sl.direction); // direction the spotlight points toward
        float theta = dot(lightDirS, spotDir);
        float epsilon = sl.innerCutoff - sl.outerCutoff;
        float intensity = clamp((theta - sl.outerCutoff) / max(epsilon, 0.001), 0.0, 1.0);

        // Specular
        float specularStrength = 0.6;
        vec3 reflectDir = reflect(-lightDirS, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
        vec3 specular = specularStrength * spec * sl.color * sl.intensity * intensity;

        vec3 diffuse = diff * sl.color * sl.intensity * intensity;
        diffuse *= attenuation;
        specular *= attenuation;
        vec3 ambient = 0.03 * sl.color * sl.intensity * attenuation;

        // Apply shadow for primary spotlight (index 0) if available
        if (i == 0) {
            vec4 fragPosLightSpace = spotLightSpace * vec4(FragPos, 1.0);
            float shadow = ShadowCalculation(fragPosLightSpace);
            diffuse *= (1.0 - shadow);
            specular *= (1.0 - shadow);
        }

        result += (ambient + diffuse + specular) * objectColorFinal;
    }

    // Slight fog for distance
    float distanceToCamera = length(viewPos - FragPos);
    float fogFactor = exp(-pow(distanceToCamera / 200.0, 2.0));
    vec3 finalColor = mix(vec3(0.05, 0.05, 0.08), result, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}