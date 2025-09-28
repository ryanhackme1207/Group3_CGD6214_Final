#pragma once

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>

class Shader; // forward decl

class Mesh {
public:
    Mesh();
    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
    // New constructor that takes an optional texture file path
    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, const std::string& texturePath);
    ~Mesh();

    void Draw(Shader& shader, const glm::mat4& modelMatrix);

    // Set or replace texture after creation
    void SetTexture(const std::string& texturePath);

    static Mesh CreateCube();

private:
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    unsigned int indexCount;
    GLuint textureID; // 0 if none

    // helper to load texture
    GLuint LoadTextureFromFile(const std::string& path);
};
