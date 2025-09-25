#include "CityManager.h"
#include <random>
#include <algorithm>
#include <tuple>
#include <ctime>

CityManager::CityManager() {
    srand(static_cast<unsigned>(time(nullptr)));
}

CityManager::~CityManager() {
    // Unique pointers handle cleanup automatically
}

void CityManager::initializeCity() {
    createBuildings();
    createRoads();
    spawnTrees(50);
    spawnGrass(30);
    spawnHumans(25);
    spawnCars(15);
    spawnStreetFurniture(20);
}

void CityManager::createBuildings() {
    // Ground
    auto ground = std::make_unique<Building>(glm::vec3(0, -1, 0), glm::vec3(60, 0.2, 60), BuildingType::FIELD);
    cityRoot.addChild(ground.get());
    buildings.push_back(std::move(ground));

    // City blocks with various buildings
    std::vector<glm::vec3> buildingPositions = {
        {-20, 0, -20}, {-10, 0, -20}, {0, 0, -20}, {10, 0, -20}, {20, 0, -20},
        {-20, 0, -10}, {-10, 0, -10}, {10, 0, -10}, {20, 0, -10},
        {-20, 0, 0}, {20, 0, 0},
        {-20, 0, 10}, {-10, 0, 10}, {10, 0, 10}, {20, 0, 10},
        {-20, 0, 20}, {-10, 0, 20}, {0, 0, 20}, {10, 0, 20}, {20, 0, 20}
    };

    for (const auto& pos : buildingPositions) {
        BuildingType type;
        glm::vec3 scale;
        float height = 3.0f + (rand() % 15);

        int buildingTypeRand = rand() % 100;
        if (buildingTypeRand < 30) {
            type = BuildingType::HOUSE;
            scale = glm::vec3(4, height * 0.6f, 4);
        }
        else if (buildingTypeRand < 60) {
            type = BuildingType::SHOP;
            scale = glm::vec3(5, height * 0.8f, 5);
        }
        else {
            type = BuildingType::SKYSCRAPER;
            scale = glm::vec3(6, height, 6);
        }

        auto building = std::make_unique<Building>(pos, scale, type);
        cityRoot.addChild(building.get());
        buildings.push_back(std::move(building));
    }
}

void CityManager::createRoads() {
    // Main roads - using proper tuple construction
    std::vector<std::tuple<glm::vec3, glm::vec3>> roads = {
        // Horizontal roads
        std::make_tuple(glm::vec3(0, -0.9, -25), glm::vec3(50, 0.1, 4)),
        std::make_tuple(glm::vec3(0, -0.9, -5), glm::vec3(50, 0.1, 4)),
        std::make_tuple(glm::vec3(0, -0.9, 5), glm::vec3(50, 0.1, 4)),
        std::make_tuple(glm::vec3(0, -0.9, 25), glm::vec3(50, 0.1, 4)),

        // Vertical roads
        std::make_tuple(glm::vec3(-25, -0.9, 0), glm::vec3(4, 0.1, 50)),
        std::make_tuple(glm::vec3(-5, -0.9, 0), glm::vec3(4, 0.1, 50)),
        std::make_tuple(glm::vec3(5, -0.9, 0), glm::vec3(4, 0.1, 50)),
        std::make_tuple(glm::vec3(25, -0.9, 0), glm::vec3(4, 0.1, 50))
    };

    for (const auto& roadData : roads) {
        auto road = std::make_unique<Building>(std::get<0>(roadData), std::get<1>(roadData), BuildingType::ROAD);
        cityRoot.addChild(road.get());
        buildings.push_back(std::move(road));
    }
}

void CityManager::spawnHumans(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = getRandomPositionOnSidewalk();
        if (isPositionValid(pos, 0.5f)) {
            auto human = std::make_unique<Entity>(pos, glm::vec3(0.4f, 1.8f, 0.4f), EntityType::HUMAN);
            human->moveRandomly(); // Start with random movement
            cityRoot.addChild(human.get());
            entities.push_back(std::move(human));
        }
    }
}

void CityManager::spawnCars(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = getRandomPositionOnRoad();
        if (isPositionValid(pos, 2.0f)) {
            auto car = std::make_unique<Entity>(pos, glm::vec3(2.0f, 1.0f, 4.0f), EntityType::CAR);
            car->moveRandomly();
            cityRoot.addChild(car.get());
            entities.push_back(std::move(car));
        }
    }
}

void CityManager::spawnTrees(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = getRandomPositionInField();
        if (isPositionValid(pos, 2.0f)) {
            float height = 3.0f + (rand() % 400) / 100.0f;
            auto tree = std::make_unique<Entity>(pos, glm::vec3(1.5f, height, 1.5f), EntityType::TREE);
            cityRoot.addChild(tree.get());
            entities.push_back(std::move(tree));
        }
    }
}

void CityManager::spawnGrass(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = getRandomPositionInField();
        if (isPositionValid(pos, 0.8f)) {
            float size = 0.5f + (rand() % 200) / 100.0f;
            auto grass = std::make_unique<Entity>(pos, glm::vec3(size, 0.3f, size), EntityType::GRASS_PATCH);
            cityRoot.addChild(grass.get());
            entities.push_back(std::move(grass));
        }
    }
}

void CityManager::spawnStreetFurniture(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = getRandomPositionOnSidewalk();
        if (isPositionValid(pos, 1.0f)) {
            EntityType type = (rand() % 2 == 0) ? EntityType::LAMP_POST : EntityType::TRASH_BIN;
            glm::vec3 scale = (type == EntityType::LAMP_POST) ?
                glm::vec3(0.3f, 4.0f, 0.3f) : glm::vec3(0.8f, 1.2f, 0.8f);

            auto furniture = std::make_unique<Entity>(pos, scale, type);
            cityRoot.addChild(furniture.get());
            entities.push_back(std::move(furniture));
        }
    }
}

glm::vec3 CityManager::getRandomPositionOnSidewalk() {
    // Positions near roads but not on them
    float x = (rand() % 4000 - 2000) / 100.0f; // -20 to 20
    float z = (rand() % 4000 - 2000) / 100.0f; // -20 to 20

    // Avoid road areas (simplified)
    if (abs(x) < 2 || abs(x - 10) < 2 || abs(x + 10) < 2 ||
        abs(z) < 2 || abs(z - 10) < 2 || abs(z + 10) < 2) {
        // Offset to sidewalk
        if (abs(x) < abs(z)) {
            x += (x > 0) ? 3 : -3;
        }
        else {
            z += (z > 0) ? 3 : -3;
        }
    }

    return glm::vec3(x, 0, z);
}

glm::vec3 CityManager::getRandomPositionOnRoad() {
    // Random position on one of the roads
    int roadChoice = rand() % 8;
    float x, z;

    if (roadChoice < 4) {
        // Horizontal roads
        x = (rand() % 4000 - 2000) / 100.0f;
        float roadZ[] = { -25, -5, 5, 25 };
        z = roadZ[roadChoice] + ((rand() % 200 - 100) / 100.0f);
    }
    else {
        // Vertical roads
        z = (rand() % 4000 - 2000) / 100.0f;
        float roadX[] = { -25, -5, 5, 25 };
        x = roadX[roadChoice - 4] + ((rand() % 200 - 100) / 100.0f);
    }

    return glm::vec3(x, -0.8f, z);
}

glm::vec3 CityManager::getRandomPositionInField() {
    float x, z;
    do {
        x = (rand() % 4000 - 2000) / 100.0f;
        z = (rand() % 4000 - 2000) / 100.0f;
    } while (abs(x) < 3 && abs(z) < 3); // Avoid center roads

    return glm::vec3(x, 0, z);
}

bool CityManager::isPositionValid(const glm::vec3& pos, float radius) {
    // Check against existing entities
    for (const auto& entity : entities) {
        glm::vec3 entityPos = glm::vec3(entity->localTransform[3]);
        if (glm::distance(pos, entityPos) < radius + 1.0f) {
            return false;
        }
    }

    // Check against buildings (simplified)
    for (const auto& building : buildings) {
        glm::vec3 buildingPos = glm::vec3(building->localTransform[3]);
        if (glm::distance(pos, buildingPos) < radius + 3.0f) {
            return false;
        }
    }

    return true;
}

void CityManager::update() {
    cityRoot.update(glm::mat4(1.0f));  // update root with identity matrix


    // Update all entities with cityRoot’s world transform
    for (auto& entity : entities) {
        entity->update(cityRoot.worldTransform);
    }
}
