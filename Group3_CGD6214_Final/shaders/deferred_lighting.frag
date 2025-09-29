#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssaoTex;
uniform int useSSAO;

uniform vec3 ambientColor = vec3(0.2);

struct PointLight { vec3 position; vec3 color; float intensity; float constant; float linear; float quadratic; };
struct SpotLight { vec3 position; vec3 direction; vec3 color; float intensity; float innerCutoff; float outerCutoff; float constant; float linear; float quadratic; };

#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 16
uniform int numPointLights; uniform int numSpotLights; 
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform vec3 viewPos;

vec3 CalcPoint(PointLight L, vec3 pos, vec3 normal, vec3 baseColor){
    vec3 lightDir = L.position - pos; float dist = length(lightDir); lightDir/=dist; float diff = max(dot(normal, lightDir),0.0); float atten = 1.0/(L.constant + L.linear*dist + L.quadratic*dist*dist); vec3 radiance = L.color * L.intensity * atten; vec3 viewDir = normalize(viewPos - pos); vec3 halfDir = normalize(lightDir+viewDir); float spec = pow(max(dot(normal,halfDir),0.0), 32.0); return (diff*baseColor + spec*0.5)*radiance; }
vec3 CalcSpot(SpotLight S, vec3 pos, vec3 normal, vec3 baseColor){ vec3 lightDir = S.position - pos; float dist=length(lightDir); lightDir/=dist; float theta = dot(-lightDir, normalize(S.direction)); float epsilon = S.innerCutoff - S.outerCutoff; float falloff = clamp((theta - S.outerCutoff)/epsilon,0.0,1.0); float diff = max(dot(normal,lightDir),0.0); float atten = 1.0/(S.constant + S.linear*dist + S.quadratic*dist*dist); vec3 radiance = S.color * S.intensity * atten * falloff; vec3 viewDir = normalize(viewPos - pos); vec3 halfDir = normalize(lightDir+viewDir); float spec = pow(max(dot(normal,halfDir),0.0), 32.0); return (diff*baseColor + spec*0.5)*radiance; }

void main(){
    vec3 pos = texture(gPosition, TexCoords).rgb; vec3 normal = normalize(texture(gNormal, TexCoords).rgb); vec4 albedoSpec = texture(gAlbedoSpec, TexCoords); vec3 albedo = albedoSpec.rgb; float specStrength = albedoSpec.a; if(length(normal)==0.0){ FragColor = vec4(0,0,0,1); return; }
    float ao = 1.0; if(useSSAO==1){ ao = texture(ssaoTex, TexCoords).r; }
    vec3 result = ambientColor * albedo * ao;
    for(int i=0;i<numPointLights;i++){ result += CalcPoint(pointLights[i], pos, normal, albedo); }
    for(int i=0;i<numSpotLights;i++){ result += CalcSpot(spotLights[i], pos, normal, albedo); }
    FragColor = vec4(result,1.0);
}
