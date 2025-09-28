#include "LODManager.h"
#include <algorithm>

LODManager::LODManager() {}
LODManager::~LODManager() {}

std::shared_ptr<Mesh> LODManager::SelectLOD(const std::vector<std::shared_ptr<Mesh>>& levels, const glm::vec3& cameraPos, const glm::vec3& objectPos) {
    if (levels.empty()) return nullptr;
    float dist = glm::distance(cameraPos, objectPos);
    size_t idx = 0;
    if (dist > 200.0f) idx = std::min<size_t>(levels.size() - 1, 2);
    else if (dist > 100.0f) idx = std::min<size_t>(levels.size() - 1, 1);
    else idx = 0;
    return levels[idx];
}
