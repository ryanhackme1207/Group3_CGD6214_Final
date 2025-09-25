#pragma once
#include <vector>
#include <memory>
#include "Node.h"
#include "Building.h"
#include "Entity.h"

class CityManager {
public:
    Node cityRoot;
    std::vector<std::unique_ptr<Building>> buildings;
    std::vector<std::unique_ptr<Entity>> entities;

    CityManager();
    ~CityManager();

    void initializeCity();
    void update();
    void spawnHumans(int count);
    void spawnCars(int count);
    void spawnTrees(int count);
    void spawnGrass(int count);
    void spawnStreetFurniture(int count);

private:
    void createBuildings();
    void createRoads();
    glm::vec3 getRandomPositionOnSidewalk();
    glm::vec3 getRandomPositionOnRoad();
    glm::vec3 getRandomPositionInField();
    bool isPositionValid(const glm::vec3& pos, float radius = 1.0f);
};