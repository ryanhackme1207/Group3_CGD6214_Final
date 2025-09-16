#include "Building.h"

Building::Building() {
    type = BuildingType::HOUSE;
    color = glm::vec3(1.0f);
    localTransform = glm::mat4(1.0f);
    worldTransform = glm::mat4(1.0f);
}

Building::Building(const glm::vec3& position, const glm::vec3& scale, BuildingType t) {
    type = t;
    localTransform = glm::mat4(1.0f);
    localTransform = glm::translate(localTransform, position);
    localTransform = glm::scale(localTransform, scale);
    worldTransform = glm::mat4(1.0f);

    switch (type) {
    case BuildingType::HOUSE:       color = glm::vec3(0.8f, 0.5f, 0.3f); break;
    case BuildingType::SHOP:        color = glm::vec3(0.2f, 0.8f, 0.2f); break;
    case BuildingType::SKYSCRAPER:  color = glm::vec3(0.5f, 0.5f, 0.8f); break;
    case BuildingType::TREE:        color = glm::vec3(0.0f, 0.6f, 0.0f); break;
    case BuildingType::FIELD:       color = glm::vec3(0.3f, 0.7f, 0.3f); break;
    case BuildingType::ROAD:        color = glm::vec3(0.15f, 0.15f, 0.17f); break;
    case BuildingType::CAR:         color = glm::vec3(1.0f, 0.0f, 0.0f); break;
    case BuildingType::MOUNTAIN:    color = glm::vec3(0.4f, 0.3f, 0.25f); break;
    default:                        color = glm::vec3(1.0f); break;
    }
}

Building::~Building() {
    // nothing to free here
}
