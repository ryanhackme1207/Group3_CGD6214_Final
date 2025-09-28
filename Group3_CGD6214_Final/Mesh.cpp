#include "Mesh.h"
#include "Shader.h"
#include <iostream>
#include <algorithm>

#include "stb_image.h"

Mesh::Mesh() : VAO(0), VBO(0), EBO(0), indexCount(0), textureID(0) {}

Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
    : VAO(0), VBO(0), EBO(0), indexCount(0), textureID(0)
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

// New constructor that attempts to load a texture
Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, const std::string& texturePath)
    : Mesh(vertices, indices)
{
    if (!texturePath.empty()) {
        textureID = LoadTextureFromFile(texturePath);
        if (textureID == 0) {
            std::cerr << "Warning: failed to load texture: " << texturePath << std::endl;
        }
    }
}

Mesh::~Mesh()
{
    if (textureID) glDeleteTextures(1, &textureID);
    if (EBO) glDeleteBuffers(1, &EBO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (VAO) glDeleteVertexArrays(1, &VAO);
}

void Mesh::SetTexture(const std::string& texturePath)
{
    if (textureID) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
    if (!texturePath.empty()) {
        textureID = LoadTextureFromFile(texturePath);
        if (textureID == 0) std::cerr << "SetTexture: failed to load " << texturePath << std::endl;
    }
}

void Mesh::Draw(Shader& shader, const glm::mat4& modelMatrix)
{
    if (VAO == 0) return;

    shader.SetMat4("model", modelMatrix);
    if (textureID) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        shader.SetInt("diffuseTex", 0);
        shader.SetBool("hasTexture", true);
    }
    else {
        // Ensure non-textured meshes are visible by providing a default object color
        // If the caller previously set a specific objectColor, this will override it only for meshes
        // without textures to avoid rendering them completely black (default uniform value is vec3(0)).
        shader.SetVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        shader.SetBool("hasTexture", false);
    }
    glBindVertexArray(VAO);
    if (indexCount > 0) {
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);

    // Unbind texture and clear hasTexture uniform to avoid affecting subsequent procedural draws
    if (textureID) {
        glBindTexture(GL_TEXTURE_2D, 0);
        shader.SetBool("hasTexture", false);
    }
}

GLuint Mesh::LoadTextureFromFile(const std::string& path)
{
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture file: " << path << std::endl;
        return 0;
    }
    GLenum format = GL_RGB;
    if (nrChannels == 1) format = GL_RED;
    else if (nrChannels == 3) format = GL_RGB;
    else if (nrChannels == 4) format = GL_RGBA;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
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
    std::vector<unsigned int> indices = { 0,1,2, 0,2,3 };
    return Mesh(vertices, indices);
}
