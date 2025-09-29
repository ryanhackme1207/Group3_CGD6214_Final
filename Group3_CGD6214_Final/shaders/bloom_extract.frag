#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform float threshold; // brightness threshold

void main(){
    vec3 color = texture(scene, TexCoords).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 result = (brightness > threshold) ? color : vec3(0.0);
    FragColor = vec4(result, 1.0);
}
