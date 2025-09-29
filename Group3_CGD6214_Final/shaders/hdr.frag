#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform sampler2D bloomBlur; // blurred bright texture
uniform float exposure;
uniform int useBloom;
uniform float bloomIntensity;

void main()
{
    const float gamma = 2.2;
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;
    if(useBloom==1){
        vec3 bloom = texture(bloomBlur, TexCoords).rgb * bloomIntensity;
        hdrColor += bloom; // additive
    }
    // Reinhard tone mapping
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    // Gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));
    FragColor = vec4(mapped, 1.0);
}