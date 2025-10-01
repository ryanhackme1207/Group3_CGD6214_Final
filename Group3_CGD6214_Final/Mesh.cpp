#include "Mesh.h"
#include "Shader.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <fstream>

#include "stb_image.h"

Mesh::Mesh() : VAO(0), VBO(0), EBO(0), indexCount(0), textureID(0), normalMapID(0), specularMapID(0), emissionMapID(0) {}

Mesh::Mesh(Mesh&& other) noexcept : VAO(other.VAO), VBO(other.VBO), EBO(other.EBO), indexCount(other.indexCount), textureID(other.textureID), normalMapID(other.normalMapID), specularMapID(other.specularMapID), emissionMapID(other.emissionMapID), cpuVertices(std::move(other.cpuVertices)), cpuIndices(std::move(other.cpuIndices)) {
    other.VAO = other.VBO = other.EBO = 0; other.textureID=other.normalMapID=other.specularMapID=other.emissionMapID=0; other.indexCount=0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if(this!=&other){
        if (emissionMapID) glDeleteTextures(1, &emissionMapID);
        if (specularMapID) glDeleteTextures(1, &specularMapID);
        if (normalMapID) glDeleteTextures(1, &normalMapID);
        if (textureID) glDeleteTextures(1, &textureID);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        VAO=other.VAO; VBO=other.VBO; EBO=other.EBO; indexCount=other.indexCount; textureID=other.textureID; normalMapID=other.normalMapID; specularMapID=other.specularMapID; emissionMapID=other.emissionMapID; cpuVertices=std::move(other.cpuVertices); cpuIndices=std::move(other.cpuIndices);
        other.VAO=other.VBO=other.EBO=0; other.textureID=other.normalMapID=other.specularMapID=other.emissionMapID=0; other.indexCount=0;
    }
    return *this;
}

static Mesh::Vertex ConvertFromFlat(const float* base) {
    Mesh::Vertex v;
    v.pos = glm::vec3(base[0], base[1], base[2]);
    v.normal = glm::vec3(base[3], base[4], base[5]);
    v.uv = glm::vec2(base[6], base[7]);
    // default tangent (may be recomputed later)
    v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    return v;
}

static void ConvertToFlat(const Mesh::Vertex& v, float* out) {
    out[0] = v.pos.x; out[1] = v.pos.y; out[2] = v.pos.z;
    out[3] = v.normal.x; out[4] = v.normal.y; out[5] = v.normal.z;
    out[6] = v.uv.x; out[7] = v.uv.y;
}

Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
    : VAO(0), VBO(0), EBO(0), indexCount(0), textureID(0), normalMapID(0), specularMapID(0), emissionMapID(0)
{
    if (vertices.empty()) return;

    // parse flat float array (assume 8 floats per vertex: pos(3), normal(3), uv(2))
    size_t vertCount = vertices.size() / 8;
    cpuVertices.reserve(vertCount);
    for (size_t i = 0; i < vertCount; ++i) {
        cpuVertices.push_back(ConvertFromFlat(&vertices[i * 8]));
    }
    cpuIndices = indices;

    // compute simple tangents per-vertex based on UVs and positions
    // zero tangents
    for (auto &v : cpuVertices) v.tangent = glm::vec3(0.0f);
    for (size_t t = 0; t + 2 < cpuIndices.size(); t += 3) {
        unsigned int i0 = cpuIndices[t+0];
        unsigned int i1 = cpuIndices[t+1];
        unsigned int i2 = cpuIndices[t+2];
        if (i0 >= cpuVertices.size() || i1 >= cpuVertices.size() || i2 >= cpuVertices.size()) continue;
        auto &v0 = cpuVertices[i0];
        auto &v1 = cpuVertices[i1];
        auto &v2 = cpuVertices[i2];
        glm::vec3 edge1 = v1.pos - v0.pos;
        glm::vec3 edge2 = v2.pos - v0.pos;
        glm::vec2 deltaUV1 = v1.uv - v0.uv;
        glm::vec2 deltaUV2 = v2.uv - v0.uv;
        float f = 1.0f;
        float denom = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        if (fabs(denom) > 1e-8f) f = 1.0f / denom; else f = 0.0f;
        glm::vec3 tangent = f * (edge1 * deltaUV2.y - edge2 * deltaUV1.y);
        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;
    }
    for (auto &v : cpuVertices) {
        if (glm::length(v.tangent) > 1e-6f) v.tangent = glm::normalize(v.tangent);
        else v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // create GPU buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    UpdateGPU();
}

// New constructor that attempts to load a diffuse texture and associated maps
Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, const std::string& texturePath)
    : Mesh(vertices, indices)
{
    if (!texturePath.empty()) {
        textureID = LoadTextureFromFile(texturePath);
        if (textureID == 0) {
            std::cerr << "Warning: failed to load texture: " << texturePath << std::endl;
        } else {
            // attempt to load associated normal, specular and emission maps using naming conventions
            normalMapID = LoadAssociatedNormalMap(texturePath);
            specularMapID = LoadAssociatedSpecularMap(texturePath);
            emissionMapID = LoadAssociatedEmissionMap(texturePath);
            // missing maps are non-fatal
        }
    }
}

Mesh::~Mesh()
{
    if (emissionMapID) glDeleteTextures(1, &emissionMapID);
    if (specularMapID) glDeleteTextures(1, &specularMapID);
    if (normalMapID) glDeleteTextures(1, &normalMapID);
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
    if (normalMapID) { glDeleteTextures(1, &normalMapID); normalMapID = 0; }
    if (specularMapID) { glDeleteTextures(1, &specularMapID); specularMapID = 0; }
    if (emissionMapID) { glDeleteTextures(1, &emissionMapID); emissionMapID = 0; }
    if (!texturePath.empty()) {
        textureID = LoadTextureFromFile(texturePath);
        if (textureID == 0) std::cerr << "SetTexture: failed to load " << texturePath << std::endl;
        else {
            normalMapID = LoadAssociatedNormalMap(texturePath);
            specularMapID = LoadAssociatedSpecularMap(texturePath);
            emissionMapID = LoadAssociatedEmissionMap(texturePath);
        }
    }
}

void Mesh::SetNormalMap(const std::string& path) {
    if (normalMapID) { glDeleteTextures(1, &normalMapID); normalMapID = 0; }
    if (!path.empty()) normalMapID = LoadTextureFromFile(path);
}

void Mesh::SetSpecularMap(const std::string& path) {
    if (specularMapID) { glDeleteTextures(1, &specularMapID); specularMapID = 0; }
    if (!path.empty()) specularMapID = LoadTextureFromFile(path);
}

void Mesh::SetEmissionMap(const std::string& path) {
    if (emissionMapID) { glDeleteTextures(1, &emissionMapID); emissionMapID = 0; }
    if (!path.empty()) emissionMapID = LoadTextureFromFile(path);
}

void Mesh::UpdateGPU()
{
    // prepare flat floats (we only upload position, normal, uv, tangent)
    std::vector<float> flat;
    flat.reserve(cpuVertices.size() * 11); // pos(3) normal(3) uv(2) tangent(3)
    for (const auto& v : cpuVertices) {
        flat.push_back(v.pos.x); flat.push_back(v.pos.y); flat.push_back(v.pos.z);
        flat.push_back(v.normal.x); flat.push_back(v.normal.y); flat.push_back(v.normal.z);
        flat.push_back(v.uv.x); flat.push_back(v.uv.y);
        flat.push_back(v.tangent.x); flat.push_back(v.tangent.y); flat.push_back(v.tangent.z);
    }

    indexCount = static_cast<unsigned int>(cpuIndices.size());

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, flat.size() * sizeof(float), flat.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, cpuIndices.size() * sizeof(unsigned int), cpuIndices.data(), GL_STATIC_DRAW);

    GLsizei stride = 11 * sizeof(float);
    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    // texcoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    // tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));

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

    // bind normal map
    if (normalMapID) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normalMapID);
        shader.SetInt("normalMap", 1);
        shader.SetBool("useNormalMap", true);
    } else {
        shader.SetBool("useNormalMap", false);
    }

    // bind specular map
    if (specularMapID) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, specularMapID);
        shader.SetInt("specularMap", 2);
        shader.SetBool("useSpecularMap", true);
    } else {
        shader.SetBool("useSpecularMap", false);
    }

    // bind emission map
    if (emissionMapID) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, emissionMapID);
        shader.SetInt("emissionMap", 3);
        shader.SetBool("useEmissionMap", true);
    } else {
        shader.SetBool("useEmissionMap", false);
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
    if (normalMapID) {
        glBindTexture(GL_TEXTURE_2D, 0);
        shader.SetBool("useNormalMap", false);
    }
    if (specularMapID) {
        glBindTexture(GL_TEXTURE_2D, 0);
        shader.SetBool("useSpecularMap", false);
    }
    if (emissionMapID) {
        glBindTexture(GL_TEXTURE_2D, 0);
        shader.SetBool("useEmissionMap", false);
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

// Attempt to load associated normal map using naming conventions 
GLuint Mesh::LoadAssociatedNormalMap(const std::string& diffusePath)
{
    // try variants
    std::vector<std::string> candidates;
    // insert "_n", "_normal", "-n", "-normal"
    size_t dot = diffusePath.find_last_of('.');
    if (dot == std::string::npos) return 0;
    std::string base = diffusePath.substr(0, dot);
    std::string ext = diffusePath.substr(dot);
    candidates.push_back(base + "_n" + ext);
    candidates.push_back(base + "_normal" + ext);
    candidates.push_back(base + "-n" + ext);
    candidates.push_back(base + "-normal" + ext);
    for (const auto &c : candidates) {
        std::ifstream f(c.c_str(), std::ios::binary);
        if (f.good()) { f.close(); return LoadTextureFromFile(c); }
    }
    return 0;
}

// Attempt to load associated specular map using naming conventions 
GLuint Mesh::LoadAssociatedSpecularMap(const std::string& diffusePath)
{
    std::vector<std::string> candidates;
    size_t dot = diffusePath.find_last_of('.');
    if (dot == std::string::npos) return 0;
    std::string base = diffusePath.substr(0, dot);
    std::string ext = diffusePath.substr(dot);
    candidates.push_back(base + "_s" + ext);
    candidates.push_back(base + "_spec" + ext);
    candidates.push_back(base + "_specular" + ext);
    candidates.push_back(base + "-s" + ext);
    candidates.push_back(base + "-specular" + ext);
    for (const auto &c : candidates) {
        std::ifstream f(c.c_str(), std::ios::binary);
        if (f.good()) { f.close(); return LoadTextureFromFile(c); }
    }
    return 0;
}

// Attempt to load associated emission map using naming conventions (e.g. diffuse.png -> diffuse_e.png or _emission.png)
GLuint Mesh::LoadAssociatedEmissionMap(const std::string& diffusePath)
{
    std::vector<std::string> candidates;
    size_t dot = diffusePath.find_last_of('.');
    if (dot == std::string::npos) return 0;
    std::string base = diffusePath.substr(0, dot);
    std::string ext = diffusePath.substr(dot);
    candidates.push_back(base + "_e" + ext);
    candidates.push_back(base + "_emit" + ext);
    candidates.push_back(base + "_emission" + ext);
    candidates.push_back(base + "-e" + ext);
    candidates.push_back(base + "-emission" + ext);
    for (const auto &c : candidates) {
        std::ifstream f(c.c_str(), std::ios::binary);
        if (f.good()) { f.close(); return LoadTextureFromFile(c); }
    }
    return 0;
}

// Simple helper to create a cube mesh (returns by value; user can wrap in shared_ptr)
Mesh Mesh::CreateCube()
{
    // 24 unique vertices (4 per face) with normals and UVs
    std::vector<float> vertices = {
        // pos              // normal        // uv
        // -Z face
        -0.5f,-0.5f,-0.5f,  0,0,-1,  0,0,
         0.5f,-0.5f,-0.5f,  0,0,-1,  1,0,
         0.5f, 0.5f,-0.5f,  0,0,-1,  1,1,
        -0.5f, 0.5f,-0.5f,  0,0,-1,  0,1,
        // +Z face
        -0.5f,-0.5f, 0.5f,  0,0, 1,  0,0,
         0.5f,-0.5f, 0.5f,  0,0, 1,  1,0,
         0.5f, 0.5f, 0.5f,  0,0, 1,  1,1,
        -0.5f, 0.5f, 0.5f,  0,0, 1,  0,1,
        // -X face
        -0.5f,-0.5f,-0.5f, -1,0,0,  0,0,
        -0.5f, 0.5f,-0.5f, -1,0,0,  1,0,
        -0.5f, 0.5f, 0.5f, -1,0,0,  1,1,
        -0.5f,-0.5f, 0.5f, -1,0,0,  0,1,
        // +X face
         0.5f,-0.5f,-0.5f,  1,0,0,  0,0,
         0.5f, 0.5f,-0.5f,  1,0,0,  1,0,
         0.5f, 0.5f, 0.5f,  1,0,0,  1,1,
         0.5f,-0.5f, 0.5f,  1,0,0,  0,1,
        // -Y face
        -0.5f,-0.5f,-0.5f,  0,-1,0, 0,1,
        -0.5f,-0.5f, 0.5f,  0,-1,0, 0,0,
         0.5f,-0.5f, 0.5f,  0,-1,0, 1,0,
         0.5f,-0.5f,-0.5f, 0,-1,0, 1,1,
        // +Y face
        -0.5f, 0.5f,-0.5f,  0, 1,0, 0,1,
        -0.5f, 0.5f, 0.5f,  0, 1,0, 0,0,
         0.5f, 0.5f, 0.5f,  0, 1,0, 1,0,
         0.5f, 0.5f,-0.5f, 0, 1,0, 1,1,
    };
    std::vector<unsigned int> indices;
    indices.reserve(36);
    auto addFace=[&](unsigned int start){
        indices.push_back(start+0); indices.push_back(start+1); indices.push_back(start+2);
        indices.push_back(start+0); indices.push_back(start+2); indices.push_back(start+3);
    };
    for(unsigned int f=0; f<6; ++f) addFace(f*4);
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
            vm.tangent = glm::normalize(0.5f * (va.tangent + vb.tangent));
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
        ve.tangent = v.tangent;
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
