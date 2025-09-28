#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent; // added for normal mapping

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

out mat3 TBN; // tangent->world matrix for normal mapping

out vec3 TangentLightPos;
out vec3 TangentViewPos;
out vec3 TangentFragPos;

uniform vec3 lightPos;
uniform vec3 viewPos;

uniform bool isGround;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    // transform normal to world space using normal matrix
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    Normal = normalize(normalMatrix * aNormal);
    TexCoord = aTexCoord;

    if (isGround) {
        // provide a simple orthonormal TBN for ground (flat plane)
        vec3 T = normalize(vec3(1.0, 0.0, 0.0));
        vec3 N = normalize(vec3(0.0, 1.0, 0.0));
        vec3 B = normalize(cross(N, T));
        TBN = mat3(T, B, N);

        TangentLightPos = TBN * lightPos;
        TangentViewPos  = TBN * viewPos;
        TangentFragPos  = TBN * FragPos;
    } else {
        // compute tangent in world-space
        vec3 T = normalize(mat3(model) * aTangent);
        vec3 N = normalize(normalMatrix * aNormal);
        vec3 B = normalize(cross(N, T));
        TBN = mat3(T, B, N);

        TangentLightPos = TBN * lightPos;
        TangentViewPos  = TBN * viewPos;
        TangentFragPos  = TBN * FragPos;
    }

    gl_Position = projection * view * vec4(FragPos, 1.0);
}