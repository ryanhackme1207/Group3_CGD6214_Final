#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Model::Model() {}
Model::~Model() {}

bool Model::LoadOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ: " << path << std::endl;
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        if (!(iss >> prefix)) continue;
        if (prefix == "v") {
            glm::vec3 v; iss >> v.x >> v.y >> v.z; positions.push_back(v);
        } else if (prefix == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z; normals.push_back(n);
        } else if (prefix == "vt") {
            glm::vec2 t; iss >> t.x >> t.y; texcoords.push_back(t);
        } else if (prefix == "f") {
            std::string a,b,c;
            iss >> a >> b >> c;
            auto parseFace = [&](const std::string& token) {
                unsigned int vi=0, ti=0, ni=0;
                size_t p1 = token.find('/');
                size_t p2 = std::string::npos;
                if (p1 != std::string::npos) p2 = token.find('/', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    std::string sv = token.substr(0, p1);
                    std::string st = token.substr(p1+1, p2 - p1 - 1);
                    std::string sn = token.substr(p2+1);
                    try {
                        if (!sv.empty()) vi = static_cast<unsigned int>(std::stoul(sv));
                        if (!st.empty()) ti = static_cast<unsigned int>(std::stoul(st));
                        if (!sn.empty()) ni = static_cast<unsigned int>(std::stoul(sn));
                    } catch(...) { return; }
                } else {
                    try { vi = static_cast<unsigned int>(std::stoul(token)); } catch(...) { return; }
                }

                if (vi == 0 || vi > positions.size()) return;
                glm::vec3 pos = positions[vi-1];
                glm::vec2 uv = (ti > 0 && ti <= texcoords.size()) ? texcoords[ti-1] : glm::vec2(0.0f);
                glm::vec3 nrm = (ni > 0 && ni <= normals.size()) ? normals[ni-1] : glm::vec3(0.0f,1.0f,0.0f);
                vertices.push_back(pos.x); vertices.push_back(pos.y); vertices.push_back(pos.z);
                vertices.push_back(nrm.x); vertices.push_back(nrm.y); vertices.push_back(nrm.z);
                vertices.push_back(uv.x); vertices.push_back(uv.y);
                indices.push_back(static_cast<unsigned int>(indices.size()));
            };
            parseFace(a); parseFace(b); parseFace(c);
        }
    }

    if (!vertices.empty() && !indices.empty()) {
        auto mesh = std::make_shared<Mesh>(vertices, indices);
        meshes.push_back(mesh);
        return true;
    }
    return false;
}

void Model::Draw(Shader& shader, const glm::mat4& modelMatrix) {
    for (auto& m : meshes) m->Draw(shader, modelMatrix);
}
