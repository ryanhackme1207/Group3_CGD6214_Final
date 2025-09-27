#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform vec3 viewPos;

void main()
{
    // Create a simple grid pattern for roads
    vec2 grid = abs(fract(FragPos.xz * 0.1) - 0.5);
    float gridLine = smoothstep(0.0, 0.02, min(grid.x, grid.y));
    
    // Road markings - create lanes
    vec2 laneGrid = abs(fract(FragPos.xz * 0.05) - 0.5);
    float laneMarking = smoothstep(0.48, 0.5, max(laneGrid.x, laneGrid.y));
    
    // Base ground color (asphalt/concrete)
    vec3 groundColor = objectColor;
    
    // Add road markings (yellow/white lines)
    groundColor = mix(groundColor, vec3(0.9, 0.9, 0.1), laneMarking * 0.3);
    
    // Add subtle grid lines
    groundColor = mix(groundColor, groundColor * 0.8, (1.0 - gridLine) * 0.2);
    
    // Simple lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 diffuse = diff * lightColor * 0.5;
    
    vec3 result = (ambient + diffuse) * groundColor;
    
    FragColor = vec4(result, 1.0);
}