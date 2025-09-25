#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Node.h"

enum class BuildingType {
    HOUSE,
    SHOP,
    SKYSCRAPER,
    TREE,
    FIELD,
    ROAD,
    CAR,
    MOUNTAIN
};

class Building : public Node {
public:
    BuildingType type;
    glm::vec3 color;

    Building();
    Building(const glm::vec3& position, const glm::vec3& scale, BuildingType type);
    virtual ~Building();
};