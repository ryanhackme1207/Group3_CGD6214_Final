#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Mesh.h"

class Shader;

class Model {
public:
    Model();
    ~Model();

    // Load simple OBJ (positions, normals, texcoords, triangular faces). Returns true on success.
    bool LoadOBJ(const std::string& path);

    // Draw all meshes with a provided shader and model matrix
    void Draw(Shader& shader, const glm::mat4& modelMatrix);

    const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const { return meshes; }

private:
    std::vector<std::shared_ptr<Mesh>> meshes;
    // Optional underglow mesh (generated for visiongt only)
    std::shared_ptr<Mesh> underglowMesh;
    bool hasUnderglow = false;
};
