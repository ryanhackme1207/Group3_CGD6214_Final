#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec2 TexCoord;
in vec3 Normal;
in vec2 DudvUV;

uniform samplerCube skybox;
uniform sampler2D dudvMap; // optional distortion map
uniform sampler2D foamMap; // optional foam texture (repeatable)
uniform float time;
uniform vec3 viewPos;
uniform vec3 baseColor;
uniform vec3 sunDir; // normalized sun direction
uniform vec3 sunColor;

// water extents for foam falloff (set from CPU)
uniform vec2 waterCenter; // X,Z center
uniform vec2 waterHalfSize; // half extents X,Z

void main() {
    // compute dudv distortion
    vec2 d = vec2(0.0);
    vec2 dudvSample = texture(dudvMap, DudvUV).rg * 2.0 - 1.0;
    d = dudvSample * 0.025; // smaller perturb

    // perturb normal using dudv (reconstruct small normal perturbation)
    vec3 pert = Normal;
    pert.xy += d;
    pert = normalize(pert);

    // view direction
    vec3 V = normalize(viewPos - WorldPos);
    // reflection vector
    vec3 R = reflect(-V, pert);

    // sample environment map with reflection vector
    vec3 env = texture(skybox, R).rgb;

    // Fresnel term (Schlick)
    float f0 = 0.02; // base reflectivity for water
    float cosTheta = clamp(dot(pert, V), 0.0, 1.0);
    float F = f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);

    // shallow tint for near grazing angles
    vec3 waterTint = baseColor * 0.6;
    vec3 color = mix(waterTint, env, F);

    // specular from sun (Blinn-Phong)
    vec3 L = normalize(-sunDir);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(pert, H), 0.0), 64.0);
    color += sunColor * spec * 0.8;

    // shoreline foam: compute distance in XZ from water rectangle edge
    vec2 p = vec2(WorldPos.x, WorldPos.z) - waterCenter;
    float dx = abs(p.x) - waterHalfSize.x;
    float dz = abs(p.y) - waterHalfSize.y;
    float outside = max(dx, dz);
    // negative outside => inside water; small positive -> near shore
    float foamFactor = clamp(1.0 - smoothstep(0.0, 6.0, outside), 0.0, 1.0);

    // sample foam texture with time-based scrolling
    vec3 foam = texture(foamMap, WorldPos.xz * 0.02 + vec2(time*0.03)).rgb;
    // blend foam with water color near shore
    color = mix(color, foam * 1.2, foamFactor * 0.9);

    // apply gamma
    float alpha = 0.9;
    FragColor = vec4(pow(color, vec3(1.0/2.2)), alpha);
}
