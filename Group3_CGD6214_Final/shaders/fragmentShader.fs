#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;
in float Height;

uniform sampler2D texture1;
uniform sampler2D shadowMap;
uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float time;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;

    // Check if current fragment is in shadow
    float bias = max(0.05 * (1.0 - dot(Normal, normalize(lightPos - FragPos))), 0.005);

    // PCF (Percentage-closer filtering) for soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main()
{
    vec4 texColor = texture(texture1, TexCoord);

    // Enhanced ambient lighting with height variation
    float ambientStrength = 0.15 + 0.1 * clamp(Height / 10.0, 0.0, 1.0);
    vec3 ambient = ambientStrength * lightColor;

    // Enhanced diffuse lighting with multiple light sources
    vec3 norm = normalize(Normal);

    // Primary light source (sun)
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * 0.8;

    // Secondary light (sky/bounce light)
    vec3 skyLightDir = normalize(vec3(0.3, 1.0, 0.2));
    float skyDiff = max(dot(norm, skyLightDir), 0.0);
    vec3 skyDiffuse = skyDiff * vec3(0.4, 0.6, 1.0) * 0.3;

    // Enhanced specular lighting with fresnel effect
    float specularStrength = 0.8;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 64);

    // Fresnel effect
    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0);
    vec3 specular = (specularStrength * spec + fresnel * 0.2) * lightColor;

    // Calculate shadow
    float shadow = ShadowCalculation(FragPosLightSpace);

    // Improved fog with exponential squared falloff
    float distance = length(viewPos - FragPos);
    float fogDensity = 0.008;
    float fogFactor = exp(-(distance * fogDensity) * (distance * fogDensity));
    fogFactor = clamp(fogFactor, 0.05, 1.0);

    // Enhanced color blending with height-based tinting
    vec3 heightTint = mix(vec3(0.9, 0.85, 0.8), vec3(1.0, 1.0, 1.0), clamp(Height / 20.0, 0.0, 1.0));

    // Combine all lighting components
    vec3 lighting = ambient + (1.0 - shadow) * (diffuse + skyDiffuse) + specular;
    vec3 result = lighting * objectColor * texColor.rgb * heightTint;

    // Enhanced atmospheric perspective
    vec3 fogColor = mix(vec3(0.6, 0.8, 1.0), vec3(1.0, 0.9, 0.7),
                       0.5 + 0.5 * sin(time * 0.001)); // Day/night cycle effect
    result = mix(fogColor, result, fogFactor);

    // Tone mapping for HDR-like effect
    result = result / (result + vec3(1.0));
    result = pow(result, vec3(1.0/2.2)); // Gamma correction

    FragColor = vec4(result, 1.0);
}