#pragma once

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader; // forward decl

class Mesh {
public:
    Mesh();
    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    void Draw(Shader& shader, const glm::mat4& modelMatrix);

    static Mesh CreateCube();

private:
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    unsigned int indexCount;
};
