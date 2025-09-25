#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main()
{
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    // Fog effect for distance
    float distance = length(viewPos - FragPos);
    float fogFactor = exp(-distance * 0.01);
    fogFactor = clamp(fogFactor, 0.1, 1.0);
    
    vec4 texColor = texture(texture1, TexCoord);
    vec3 result = (ambient + diffuse + specular) * objectColor * texColor.rgb;
    result = mix(vec3(0.5, 0.7, 1.0), result, fogFactor); // Mix with sky color for fog
    
    FragColor = vec4(result, 1.0);
}