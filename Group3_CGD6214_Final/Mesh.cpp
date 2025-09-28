#include "Mesh.h"
#include "Shader.h"
#include <iostream>
#include <algorithm>

Mesh::Mesh() : VAO(0), VBO(0), EBO(0), indexCount(0) {}

Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
    : VAO(0), VBO(0), EBO(0), indexCount(0)
{
    if (vertices.empty()) return;

    indexCount = static_cast<unsigned int>(indices.size());

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Assume vertex format: position(3), normal(3), texcoord(2) => stride = 8 floats
    GLsizei stride = 8 * sizeof(float);
    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    // texcoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    if (EBO) glDeleteBuffers(1, &EBO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (VAO) glDeleteVertexArrays(1, &VAO);
}

void Mesh::Draw(Shader& shader, const glm::mat4& modelMatrix)
{
    if (VAO == 0) return;

    shader.SetMat4("model", modelMatrix);
    glBindVertexArray(VAO);
    if (indexCount > 0) {
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

// Simple helper to create a cube mesh (returns by value; user can wrap in shared_ptr)
Mesh Mesh::CreateCube()
{
    std::vector<float> vertices = {
        // positions        // normals        // texcoords
        -0.5f,-0.5f,-0.5f,  0,0,-1,  0,0,
         0.5f,-0.5f,-0.5f,  0,0,-1,  1,0,
         0.5f, 0.5f,-0.5f,  0,0,-1,  1,1,
        -0.5f, 0.5f,-0.5f,  0,0,-1,  0,1,
        // ... other faces (omitted for brevity) -- not needed for initial tests
    };
    // simple index list for a single quad (placeholder)
    std::vector<unsigned int> indices = {0,1,2, 0,2,3};
    return Mesh(vertices, indices);
}
