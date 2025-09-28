#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include "Mesh.h"

class LODManager {
public:
    LODManager();
    ~LODManager();

    // Select an LOD mesh from levels based on camera distance to object
    std::shared_ptr<Mesh> SelectLOD(const std::vector<std::shared_ptr<Mesh>>& levels, const glm::vec3& cameraPos, const glm::vec3& objectPos);
};
