#include "Mesh.h"
#include "Shader.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <cmath>

#include "stb_image.h"

Mesh::Mesh() : VAO(0), VBO(0), EBO(0), indexCount(0), textureID(0) {}

static Mesh::Vertex ConvertFromFlat(const float* base) {
    Mesh::Vertex v;
    v.pos = glm::vec3(base[0], base[1], base[2]);
    v.normal = glm::vec3(base[3], base[4], base[5]);
    v.uv = glm::vec2(base[6], base[7]);
    return v;
}

static void ConvertToFlat(const Mesh::Vertex& v, float* out) {
    out[0] = v.pos.x; out[1] = v.pos.y; out[2] = v.pos.z;
    out[3] = v.normal.x; out[4] = v.normal.y; out[5] = v.normal.z;
    out[6] = v.uv.x; out[7] = v.uv.y;
}

Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
    : VAO(0), VBO(0), EBO(0), indexCount(0), textureID(0)
{
    if (vertices.empty()) return;

    // parse flat float array (assume 8 floats per vertex: pos(3), normal(3), uv(2))
    size_t vertCount = vertices.size() / 8;
    cpuVertices.reserve(vertCount);
    for (size_t i = 0; i < vertCount; ++i) {
        cpuVertices.push_back(ConvertFromFlat(&vertices[i * 8]));
    }
    cpuIndices = indices;

    // create GPU buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    UpdateGPU();
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

void Mesh::UpdateGPU()
{
    // prepare flat floats
    std::vector<float> flat;
    flat.reserve(cpuVertices.size() * 8);
    for (const auto& v : cpuVertices) {
        float tmp[8];
        ConvertToFlat(v, tmp);
        for (int i = 0; i < 8; ++i) flat.push_back(tmp[i]);
    }

    indexCount = static_cast<unsigned int>(cpuIndices.size());

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, flat.size() * sizeof(float), flat.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, cpuIndices.size() * sizeof(unsigned int), cpuIndices.data(), GL_STATIC_DRAW);

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

unsigned int Mesh::AddVertex(const Vertex& v)
{
    cpuVertices.push_back(v);
    return static_cast<unsigned int>(cpuVertices.size() - 1);
}

void Mesh::Draw(Shader& shader, const glm::mat4& modelMatrix)
{
    if (VAO == 0 || indexCount == 0) return;

    shader.SetMat4("model", modelMatrix);
    if (textureID) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        shader.SetInt("diffuseTex", 0);
        shader.SetBool("hasTexture", true);
    }
    else {
        shader.SetVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        shader.SetBool("hasTexture", false);
    }
    glBindVertexArray(VAO);
    if (indexCount > 0) {
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);

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

// --- New mesh-edit functions ---

void Mesh::RecomputeNormals()
{
    if (cpuVertices.empty() || cpuIndices.empty()) return;
    for (auto &v : cpuVertices) v.normal = glm::vec3(0.0f);
    for (size_t t = 0; t + 2 < cpuIndices.size(); t += 3) {
        unsigned int i0 = cpuIndices[t+0];
        unsigned int i1 = cpuIndices[t+1];
        unsigned int i2 = cpuIndices[t+2];
        const glm::vec3 &p0 = cpuVertices[i0].pos;
        const glm::vec3 &p1 = cpuVertices[i1].pos;
        const glm::vec3 &p2 = cpuVertices[i2].pos;
        glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        cpuVertices[i0].normal += n;
        cpuVertices[i1].normal += n;
        cpuVertices[i2].normal += n;
    }
    for (auto &v : cpuVertices) {
        v.normal = glm::normalize(v.normal);
        if (!std::isfinite(v.normal.x) || !std::isfinite(v.normal.y) || !std::isfinite(v.normal.z)) {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

void Mesh::SubdivideMidpoint()
{
    if (cpuIndices.empty() || cpuVertices.empty()) return;

    std::unordered_map<uint64_t, unsigned int> edgeMid;
    std::vector<unsigned int> newIndices;
    newIndices.reserve(cpuIndices.size() * 4);

    auto getEdgeKey = [](unsigned int a, unsigned int b) -> uint64_t {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(a) << 32) | b;
    };

    for (size_t t = 0; t + 2 < cpuIndices.size(); t += 3) {
        unsigned int i0 = cpuIndices[t + 0];
        unsigned int i1 = cpuIndices[t + 1];
        unsigned int i2 = cpuIndices[t + 2];

        auto makeMid = [&](unsigned int a, unsigned int b) -> unsigned int {
            uint64_t key = getEdgeKey(a, b);
            auto it = edgeMid.find(key);
            if (it != edgeMid.end()) return it->second;
            const Vertex& va = cpuVertices[a];
            const Vertex& vb = cpuVertices[b];
            Vertex vm;
            vm.pos = 0.5f * (va.pos + vb.pos);
            vm.normal = glm::normalize(0.5f * (va.normal + vb.normal));
            vm.uv = 0.5f * (va.uv + vb.uv);
            unsigned int idx = AddVertex(vm);
            edgeMid[key] = idx;
            return idx;
        };

        unsigned int m01 = makeMid(i0, i1);
        unsigned int m12 = makeMid(i1, i2);
        unsigned int m20 = makeMid(i2, i0);

        // create 4 triangles
        newIndices.push_back(i0); newIndices.push_back(m01); newIndices.push_back(m20);
        newIndices.push_back(i1); newIndices.push_back(m12); newIndices.push_back(m01);
        newIndices.push_back(i2); newIndices.push_back(m20); newIndices.push_back(m12);
        newIndices.push_back(m01); newIndices.push_back(m12); newIndices.push_back(m20);
    }

    cpuIndices.swap(newIndices);

    // recompute normals and upload
    RecomputeNormals();
    UpdateGPU();
}

void Mesh::Extrude(float distance)
{
    if (cpuIndices.empty() || cpuVertices.empty()) return;
    size_t origVertexCount = cpuVertices.size();
    std::vector<int> extrudedIndex(origVertexCount, -1);

    for (size_t i = 0; i < origVertexCount; ++i) {
        Vertex v = cpuVertices[i];
        Vertex ve = v;
        ve.pos += v.normal * distance;
        extrudedIndex[i] = static_cast<int>(AddVertex(ve));
    }

    std::vector<unsigned int> newIndices;
    newIndices.reserve(cpuIndices.size() * 6);

    for (size_t t = 0; t + 2 < cpuIndices.size(); t += 3) {
        unsigned int i0 = cpuIndices[t + 0];
        unsigned int i1 = cpuIndices[t + 1];
        unsigned int i2 = cpuIndices[t + 2];
        unsigned int e0 = static_cast<unsigned int>(extrudedIndex[i0]);
        unsigned int e1 = static_cast<unsigned int>(extrudedIndex[i1]);
        unsigned int e2 = static_cast<unsigned int>(extrudedIndex[i2]);

        // sides i0-i1
        newIndices.push_back(i0); newIndices.push_back(i1); newIndices.push_back(e1);
        newIndices.push_back(i0); newIndices.push_back(e1); newIndices.push_back(e0);
        // sides i1-i2
        newIndices.push_back(i1); newIndices.push_back(i2); newIndices.push_back(e2);
        newIndices.push_back(i1); newIndices.push_back(e2); newIndices.push_back(e1);
        // sides i2-i0
        newIndices.push_back(i2); newIndices.push_back(i0); newIndices.push_back(e0);
        newIndices.push_back(i2); newIndices.push_back(e0); newIndices.push_back(e2);

        // cap (extruded face) - reverse winding
        newIndices.push_back(e0); newIndices.push_back(e2); newIndices.push_back(e1);
    }

    cpuIndices.swap(newIndices);

    RecomputeNormals();
    UpdateGPU();
}

static float pseudo_noise(float x, float y, float z, unsigned int seed) {
    float h = std::sin(x * 12.9898f + y * 78.233f + z * 37.719f + float(seed) * 0.6180339f) * 43758.5453f;
    return h - std::floor(h);
}

void Mesh::DeformDisplace(float magnitude, unsigned int seed)
{
    if (cpuVertices.empty()) return;
    for (auto& v : cpuVertices) {
        float n = pseudo_noise(v.pos.x, v.pos.y, v.pos.z, seed);
        float disp = (n * 2.0f - 1.0f) * magnitude;
        v.pos += v.normal * disp;
    }
    RecomputeNormals();
    UpdateGPU();
}
