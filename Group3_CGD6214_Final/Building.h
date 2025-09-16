#ifndef BUILDING_H
#define BUILDING_H

#include "Node.h"
#include <glm/glm.hpp>

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
    Building(const glm::vec3& position, const glm::vec3& scale, BuildingType t);
    virtual ~Building();
};

#endif // BUILDING_H
