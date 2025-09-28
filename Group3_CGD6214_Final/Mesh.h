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

    // --- CPU-side mesh and edit operations ---
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    // Subdivide each triangle into 4 by creating midpoints on edges (midpoint subdivision)
    void SubdivideMidpoint();

    // Extrude the mesh along vertex normals by distance (creates side faces and cap)
    void Extrude(float distance);

    // Displace vertices along normals by random offset in [-magnitude, +magnitude]
    void DeformDisplace(float magnitude, unsigned int seed = 0);

private:
    // GPU handles
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    unsigned int indexCount;
    GLuint textureID; // 0 if none

    // CPU-side mesh
    std::vector<Vertex> cpuVertices;
    std::vector<unsigned int> cpuIndices;

    // helper to load texture
    GLuint LoadTextureFromFile(const std::string& path);

    // upload CPU mesh to GPU buffers
    void UpdateGPU();

    // add vertex to cpu array and return its index
    unsigned int AddVertex(const Vertex& v);

    // recompute per-vertex normals from triangles
    void RecomputeNormals();
};
