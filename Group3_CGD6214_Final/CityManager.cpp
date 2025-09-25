#include "CityManager.h"
#include <random>
#include <algorithm>
#include <tuple>
#include <ctime>
#include <cmath>
#include <memory>

CityManager::CityManager() {
    srand(static_cast<unsigned>(time(nullptr)));
}

CityManager::~CityManager() {
    // Unique pointers handle cleanup automatically
}

void CityManager::initializeCity() {
    createBoundaryWalls();
    createBuildings();
    createGridBasedRoads();
    fillLandWithGrass();
    spawnTrees(30); // Reduced for smaller map
    spawnHumans(15);
    spawnCars(15);
    spawnStreetFurniture(12);
}

void CityManager::createBoundaryWalls() {
    // Create boundary walls around the entire city
    const float wallHeight = 8.0f;
    const float wallThickness = 2.0f;
    const float citySize = 50.0f; // Reduced city size

    // North wall
    auto northWall = std::make_unique<Building>(
        glm::vec3(0, wallHeight / 2, citySize),
        glm::vec3(citySize * 2 + wallThickness, wallHeight, wallThickness),
        BuildingType::MOUNTAIN
    );
    cityRoot.addChild(northWall.get());
    buildings.push_back(std::move(northWall));

    // South wall
    auto southWall = std::make_unique<Building>(
        glm::vec3(0, wallHeight / 2, -citySize),
        glm::vec3(citySize * 2 + wallThickness, wallHeight, wallThickness),
        BuildingType::MOUNTAIN
    );
    cityRoot.addChild(southWall.get());
    buildings.push_back(std::move(southWall));

    // East wall
    auto eastWall = std::make_unique<Building>(
        glm::vec3(citySize, wallHeight / 2, 0),
        glm::vec3(wallThickness, wallHeight, citySize * 2),
        BuildingType::MOUNTAIN
    );
    cityRoot.addChild(eastWall.get());
    buildings.push_back(std::move(eastWall));

    // West wall
    auto westWall = std::make_unique<Building>(
        glm::vec3(-citySize, wallHeight / 2, 0),
        glm::vec3(wallThickness, wallHeight, citySize * 2),
        BuildingType::MOUNTAIN
    );
    cityRoot.addChild(westWall.get());
    buildings.push_back(std::move(westWall));
}

void CityManager::createBuildings() {
    // Large ground plane - smaller size
    auto ground = std::make_unique<Building>(glm::vec3(0, -1.5f, 0), glm::vec3(100, 0.2, 100), BuildingType::FIELD);
    cityRoot.addChild(ground.get());
    buildings.push_back(std::move(ground));

    createDowntownDistrict();
    createResidentialAreas();
    createIndustrialZone();
    createBeachArea();
}

void CityManager::createDowntownDistrict() {
    // Downtown skyscrapers - reduced scale
    std::vector<std::tuple<glm::vec3, glm::vec3, BuildingType>> downtownBuildings = {
        // Central business district - main skyscrapers
        {{0, 0, 0}, {6, 20, 6}, BuildingType::SKYSCRAPER},
        {{8, 0, 0}, {7, 22, 7}, BuildingType::SKYSCRAPER},
        {{-8, 0, 0}, {6, 18, 6}, BuildingType::SKYSCRAPER},
        {{0, 0, 8}, {5, 16, 5}, BuildingType::SKYSCRAPER},
        {{0, 0, -8}, {7, 24, 7}, BuildingType::SKYSCRAPER},

        // Secondary downtown buildings
        {{10, 0, 10}, {4, 12, 4}, BuildingType::SHOP},
        {{-10, 0, 10}, {4, 14, 4}, BuildingType::SHOP},
        {{10, 0, -10}, {4, 11, 4}, BuildingType::SHOP},
        {{-10, 0, -10}, {5, 13, 5}, BuildingType::SHOP},

        // Additional downtown buildings
        {{15, 0, 0}, {5, 18, 5}, BuildingType::SKYSCRAPER},
        {{-15, 0, 0}, {6, 16, 6}, BuildingType::SKYSCRAPER},
        {{0, 0, 15}, {4, 14, 4}, BuildingType::SHOP},
        {{0, 0, -15}, {6, 19, 6}, BuildingType::SKYSCRAPER},

        // Mid-rise buildings
        {{6, 0, 6}, {3, 10, 3}, BuildingType::SHOP},
        {{-6, 0, 6}, {3, 11, 3}, BuildingType::SHOP},
        {{6, 0, -6}, {3, 9, 3}, BuildingType::SHOP},
        {{-6, 0, -6}, {3, 12, 3}, BuildingType::SHOP},
    };

    for (const auto& buildingData : downtownBuildings) {
        auto building = std::make_unique<Building>(std::get<0>(buildingData), std::get<1>(buildingData), std::get<2>(buildingData));
        cityRoot.addChild(building.get());
        buildings.push_back(std::move(building));
    }
}

void CityManager::createResidentialAreas() {
    // Residential neighborhoods - smaller grid
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            float x = -40.0f + i * 13.0f;
            float z = -40.0f + j * 13.0f;

            // Skip downtown area
            if (fabsf(x) < 20.0f && fabsf(z) < 20.0f) continue;

            float height = 3.0f + (rand() % 6);
            glm::vec3 scale = glm::vec3(3 + (rand() % 3), height, 3 + (rand() % 3));

            BuildingType type;
            int typeRand = rand() % 100;
            if (typeRand < 70) {
                type = BuildingType::HOUSE;
            }
            else if (typeRand < 90) {
                type = BuildingType::SHOP;
                scale.y = height * 0.8f;
            }
            else {
                type = BuildingType::SKYSCRAPER;
                scale.y = height * 1.2f;
            }

            auto building = std::make_unique<Building>(glm::vec3(x, 0, z), scale, type);
            cityRoot.addChild(building.get());
            buildings.push_back(std::move(building));

            // Add smaller buildings around main ones
            if (rand() % 100 < 25) {
                float offsetX = ((rand() % 60) - 30) / 10.0f;
                float offsetZ = ((rand() % 60) - 30) / 10.0f;
                float smallHeight = 2.0f + (rand() % 3);
                glm::vec3 smallScale = glm::vec3(2 + (rand() % 2), smallHeight, 2 + (rand() % 2));

                auto smallBuilding = std::make_unique<Building>(
                    glm::vec3(x + offsetX, 0, z + offsetZ),
                    smallScale,
                    BuildingType::HOUSE
                );
                cityRoot.addChild(smallBuilding.get());
                buildings.push_back(std::move(smallBuilding));
            }
        }
    }
}

void CityManager::createIndustrialZone() {
    // Industrial area - smaller
    for (int i = 0; i < 2; ++i) {
        float x = -40.0f + i * 12.0f;
        float z = -40.0f;

        float height = 6.0f + (rand() % 4);
        glm::vec3 scale = glm::vec3(8, height, 6);

        auto warehouse = std::make_unique<Building>(glm::vec3(x, 0, z), scale, BuildingType::SHOP);
        cityRoot.addChild(warehouse.get());
        buildings.push_back(std::move(warehouse));
    }
}

void CityManager::createBeachArea() {
    // Beach boardwalk buildings - smaller
    for (int i = 0; i < 3; ++i) {
        float x = -20.0f + i * 12.0f;
        float z = 30.0f;

        float height = 2.0f + (rand() % 3);
        glm::vec3 scale = glm::vec3(3, height, 2);

        auto beachBuilding = std::make_unique<Building>(glm::vec3(x, 0, z), scale, BuildingType::HOUSE);
        cityRoot.addChild(beachBuilding.get());
        buildings.push_back(std::move(beachBuilding));
    }
}

void CityManager::createGridBasedRoads() {
    createHighwaySystem();
    createCityStreets();
}

void CityManager::createHighwaySystem() {
    // Main highway running North-South - smaller
    auto highway_ns = std::make_unique<Building>(
        glm::vec3(25, -1.35f, 0),
        glm::vec3(6, 0.1f, 100),
        BuildingType::ROAD
    );
    cityRoot.addChild(highway_ns.get());
    buildings.push_back(std::move(highway_ns));

    // Main highway running East-West - smaller
    auto highway_ew = std::make_unique<Building>(
        glm::vec3(0, -1.35f, -25),
        glm::vec3(100, 0.1f, 6),
        BuildingType::ROAD
    );
    cityRoot.addChild(highway_ew.get());
    buildings.push_back(std::move(highway_ew));

    // Highway interchange
    auto interchange = std::make_unique<Building>(
        glm::vec3(25, -1.35f, -25),
        glm::vec3(8, 0.1f, 8),
        BuildingType::ROAD
    );
    cityRoot.addChild(interchange.get());
    buildings.push_back(std::move(interchange));
}

void CityManager::createCityStreets() {
    // Downtown street grid - smaller
    const float downtownSize = 18.0f;
    const float streetWidth = 3.0f;
    const float blockSize = 6.0f;

    // Downtown horizontal streets
    for (float z = -downtownSize; z <= downtownSize; z += blockSize) {
        auto street = std::make_unique<Building>(
            glm::vec3(0, -1.35f, z),
            glm::vec3(downtownSize * 2, 0.1f, streetWidth),
            BuildingType::ROAD
        );
        cityRoot.addChild(street.get());
        buildings.push_back(std::move(street));
    }

    // Downtown vertical streets
    for (float x = -downtownSize; x <= downtownSize; x += blockSize) {
        auto street = std::make_unique<Building>(
            glm::vec3(x, -1.35f, 0),
            glm::vec3(streetWidth, 0.1f, downtownSize * 2),
            BuildingType::ROAD
        );
        cityRoot.addChild(street.get());
        buildings.push_back(std::move(street));
    }

    // Suburban roads - smaller
    const float suburbSize = 45.0f;
    const float suburbBlockSize = 13.0f;

    // Suburban horizontal roads
    for (float z = -suburbSize; z <= suburbSize; z += suburbBlockSize) {
        if (fabsf(z) > downtownSize) {
            auto road = std::make_unique<Building>(
                glm::vec3(0, -1.35f, z),
                glm::vec3(suburbSize * 2, 0.1f, streetWidth),
                BuildingType::ROAD
            );
            cityRoot.addChild(road.get());
            buildings.push_back(std::move(road));
        }
    }

    // Suburban vertical roads
    for (float x = -suburbSize; x <= suburbSize; x += suburbBlockSize) {
        if (fabsf(x) > downtownSize) {
            auto road = std::make_unique<Building>(
                glm::vec3(x, -1.35f, 0),
                glm::vec3(streetWidth, 0.1f, suburbSize * 2),
                BuildingType::ROAD
            );
            cityRoot.addChild(road.get());
            buildings.push_back(std::move(road));
        }
    }

    // Beach coastal road
    auto coastalRoad = std::make_unique<Building>(
        glm::vec3(0, -1.35f, 35),
        glm::vec3(60, 0.1f, 4),
        BuildingType::ROAD
    );
    cityRoot.addChild(coastalRoad.get());
    buildings.push_back(std::move(coastalRoad));

    createStreetLights();
}

void CityManager::createStreetLights() {
    // Street lights - reduced for smaller map
    const float lightSpacing = 10.0f;

    // Downtown street lights
    const float downtownSize = 18.0f;
    const float blockSize = 6.0f;

    for (float z = -downtownSize; z <= downtownSize; z += blockSize) {
        for (float x = -downtownSize; x <= downtownSize; x += lightSpacing) {
            if (isBuildingArea(x, z)) continue;

            auto light1 = std::make_unique<Entity>(
                glm::vec3(x, -1.25f, z + 2.5f),
                glm::vec3(0.25f, 3.5f, 0.25f),
                EntityType::LAMP_POST
            );
            cityRoot.addChild(light1.get());
            entities.push_back(std::move(light1));

            auto light2 = std::make_unique<Entity>(
                glm::vec3(x, -1.25f, z - 2.5f),
                glm::vec3(0.25f, 3.5f, 0.25f),
                EntityType::LAMP_POST
            );
            cityRoot.addChild(light2.get());
            entities.push_back(std::move(light2));
        }
    }

    // Highway lights
    for (float z = -45.0f; z <= 45.0f; z += lightSpacing * 1.5f) {
        auto highway_light1 = std::make_unique<Entity>(
            glm::vec3(28.0f, -1.25f, z),
            glm::vec3(0.3f, 4.0f, 0.3f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(highway_light1.get());
        entities.push_back(std::move(highway_light1));

        auto highway_light2 = std::make_unique<Entity>(
            glm::vec3(22.0f, -1.25f, z),
            glm::vec3(0.3f, 4.0f, 0.3f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(highway_light2.get());
        entities.push_back(std::move(highway_light2));
    }
}

void CityManager::fillLandWithGrass() {
    // Create comprehensive terrain coverage with no gaps
    const float terrainSize = 50.0f; // Match city boundary
    const float grassTileSize = 2.0f; // Smaller tiles for better coverage

    // Create overlapping grid to ensure no gaps
    for (float x = -terrainSize; x <= terrainSize; x += grassTileSize) {
        for (float z = -terrainSize; z <= terrainSize; z += grassTileSize) {
            // Create base terrain tile
            glm::vec3 terrainPos = glm::vec3(x, -1.46f, z);
            glm::vec3 tileSize = glm::vec3(grassTileSize * 1.5f, 0.02f, grassTileSize * 1.5f); // Overlapping

            BuildingType terrainType = BuildingType::FIELD;

            // Beach areas
            if (z > 32.0f) {
                tileSize.y = 0.01f;
            }
            // Downtown areas
            else if (fabsf(x) < 20.0f && fabsf(z) < 20.0f) {
                tileSize.y = 0.015f;
            }

            auto terrainTile = std::make_unique<Building>(terrainPos, tileSize, terrainType);
            cityRoot.addChild(terrainTile.get());
            buildings.push_back(std::move(terrainTile));
        }
    }

    // Add additional layer with offset for complete coverage
    for (float x = -terrainSize + grassTileSize / 2; x <= terrainSize; x += grassTileSize) {
        for (float z = -terrainSize + grassTileSize / 2; z <= terrainSize; z += grassTileSize) {
            if (isRoadArea(x, z)) continue;

            glm::vec3 terrainPos = glm::vec3(x, -1.45f, z);
            glm::vec3 tileSize = glm::vec3(grassTileSize * 1.3f, 0.015f, grassTileSize * 1.3f);

            auto fillTile = std::make_unique<Building>(terrainPos, tileSize, BuildingType::FIELD);
            cityRoot.addChild(fillTile.get());
            buildings.push_back(std::move(fillTile));
        }
    }

    createParks();
    createNaturalFeatures();
}

void CityManager::createParks() {
    // Central park - smaller
    for (int i = 0; i < 12; ++i) {
        float x = -6.0f + ((rand() % 100) / 100.0f) * 12.0f;
        float z = 6.0f + ((rand() % 100) / 100.0f) * 6.0f;

        float size = 0.8f + (rand() % 40) / 100.0f;
        auto parkGrass = std::make_unique<Entity>(
            glm::vec3(x, -0.82f, z),
            glm::vec3(size, 0.25f, size),
            EntityType::GRASS_PATCH
        );
        cityRoot.addChild(parkGrass.get());
        entities.push_back(std::move(parkGrass));
    }

    // Suburban parks - smaller
    std::vector<glm::vec2> parkCenters = { {-30.0f, -30.0f}, {30.0f, 30.0f} };

    for (const auto& center : parkCenters) {
        for (int i = 0; i < 8; ++i) {
            float x = center.x + ((rand() % 100) / 100.0f - 0.5f) * 15.0f;
            float z = center.y + ((rand() % 100) / 100.0f - 0.5f) * 15.0f;

            if (isRoadArea(x, z) || isBuildingArea(x, z)) continue;

            float size = 1.0f + (rand() % 30) / 100.0f;
            auto parkFeature = std::make_unique<Entity>(
                glm::vec3(x, -0.82f, z),
                glm::vec3(size, 0.2f, size),
                EntityType::GRASS_PATCH
            );
            cityRoot.addChild(parkFeature.get());
            entities.push_back(std::move(parkFeature));
        }
    }
}

void CityManager::createNaturalFeatures() {
    // Beach vegetation - smaller area
    for (int i = 0; i < 15; ++i) {
        float x = -40.0f + ((rand() % 100) / 100.0f) * 80.0f;
        float z = 33.0f + ((rand() % 100) / 100.0f) * 6.0f;

        if (isRoadArea(x, z)) continue;

        float size = 0.6f + (rand() % 30) / 100.0f;
        auto beachGrass = std::make_unique<Entity>(
            glm::vec3(x, -0.83f, z),
            glm::vec3(size, 0.12f, size),
            EntityType::GRASS_PATCH
        );
        cityRoot.addChild(beachGrass.get());
        entities.push_back(std::move(beachGrass));
    }
}

void CityManager::spawnHumans(int count) {
    int spawnedCount = 0;
    int maxAttempts = count * 5; // Try up to 5 times per human

    for (int attempts = 0; attempts < maxAttempts && spawnedCount < count; ++attempts) {
        glm::vec3 pos = getRandomPositionOnSidewalk();
        if (isPositionValid(pos, 1.0f)) { // Reduced radius for less strict validation
            auto human = std::make_unique<Entity>(pos, glm::vec3(0.4f, 1.8f, 0.4f), EntityType::HUMAN);
            human->moveRandomly();
            cityRoot.addChild(human.get());
            entities.push_back(std::move(human));
            spawnedCount++;
        }
    }
}

void CityManager::spawnCars(int count) {
    int spawnedCount = 0;
    int maxAttempts = count * 5; // Try up to 5 times per car

    for (int attempts = 0; attempts < maxAttempts && spawnedCount < count; ++attempts) {
        glm::vec3 pos = getRandomPositionOnRoad();
        if (isPositionValid(pos, 2.5f)) { // Slightly larger radius for cars
            auto car = std::make_unique<Entity>(pos, glm::vec3(2.0f, 1.0f, 4.0f), EntityType::CAR);
            car->moveRandomly();
            cityRoot.addChild(car.get());
            entities.push_back(std::move(car));
            spawnedCount++;
        }
    }
}

void CityManager::spawnTrees(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = getRandomPositionInField();
        if (isPositionValid(pos, 2.0f)) {
            float height = 2.5f + (rand() % 300) / 100.0f;
            auto tree = std::make_unique<Entity>(pos, glm::vec3(1.2f, height, 1.2f), EntityType::TREE);
            cityRoot.addChild(tree.get());
            entities.push_back(std::move(tree));
        }
    }
}

void CityManager::spawnStreetFurniture(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = getRandomPositionOnSidewalk();
        if (isPositionValid(pos, 1.0f)) {
            EntityType type = (rand() % 2 == 0) ? EntityType::LAMP_POST : EntityType::TRASH_BIN;
            glm::vec3 scale = (type == EntityType::LAMP_POST) ?
                glm::vec3(0.25f, 3.5f, 0.25f) : glm::vec3(0.6f, 1.0f, 0.6f);

            auto furniture = std::make_unique<Entity>(pos, scale, type);
            cityRoot.addChild(furniture.get());
            entities.push_back(std::move(furniture));
        }
    }
}

glm::vec3 CityManager::getRandomPositionOnSidewalk() {
    // Generate positions with more variety and better spread
    float x, z;
    int areaChoice = rand() % 3;

    if (areaChoice == 0) {
        // Downtown sidewalks
        x = -15.0f + ((rand() % 100) / 100.0f) * 30.0f;
        z = -15.0f + ((rand() % 100) / 100.0f) * 30.0f;

        // Move to sidewalk if too close to road centers
        if (fmod(abs((int)x), 6) < 2) x += (x > 0) ? 3.0f : -3.0f;
        if (fmod(abs((int)z), 6) < 2) z += (z > 0) ? 3.0f : -3.0f;
    }
    else if (areaChoice == 1) {
        // Suburban areas
        x = -40.0f + ((rand() % 100) / 100.0f) * 80.0f;
        z = -40.0f + ((rand() % 100) / 100.0f) * 80.0f;

        // Skip downtown core
        if (abs(x) < 20.0f && abs(z) < 20.0f) {
            x += (x > 0) ? 25.0f : -25.0f;
        }
    }
    else {
        // Beach area
        x = -30.0f + ((rand() % 100) / 100.0f) * 60.0f;
        z = 30.0f + ((rand() % 100) / 100.0f) * 10.0f;
    }

    return glm::vec3(x, -0.9f, z);
}

glm::vec3 CityManager::getRandomPositionOnRoad() {
    // Generate road positions with better variety
    int roadType = rand() % 3;
    float x, z;

    if (roadType == 0) {
        // Downtown roads
        const float blockSize = 6.0f;
        std::vector<float> roadPositions;
        for (float pos = -18.0f; pos <= 18.0f; pos += blockSize) {
            roadPositions.push_back(pos);
        }

        if (rand() % 2 == 0) {
            // Horizontal road
            x = -15.0f + ((rand() % 100) / 100.0f) * 30.0f;
            int roadIndex = rand() % roadPositions.size();
            z = roadPositions[roadIndex];
        }
        else {
            // Vertical road
            z = -15.0f + ((rand() % 100) / 100.0f) * 30.0f;
            int roadIndex = rand() % roadPositions.size();
            x = roadPositions[roadIndex];
        }
    }
    else if (roadType == 1) {
        // Highway
        if (rand() % 2 == 0) {
            // North-South highway
            x = 25.0f + ((rand() % 100 - 50) / 50.0f) * 2.0f; // Small variation
            z = -45.0f + ((rand() % 100) / 100.0f) * 90.0f;
        }
        else {
            // East-West highway
            z = -25.0f + ((rand() % 100 - 50) / 50.0f) * 2.0f; // Small variation
            x = -45.0f + ((rand() % 100) / 100.0f) * 90.0f;
        }
    }
    else {
        // Suburban roads
        const float suburbBlockSize = 13.0f;
        std::vector<float> suburbRoadPositions;
        for (float pos = -45.0f; pos <= 45.0f; pos += suburbBlockSize) {
            if (abs(pos) > 18.0f) { // Outside downtown
                suburbRoadPositions.push_back(pos);
            }
        }

        if (!suburbRoadPositions.empty()) {
            if (rand() % 2 == 0) {
                // Horizontal suburban road
                x = -40.0f + ((rand() % 100) / 100.0f) * 80.0f;
                int roadIndex = rand() % suburbRoadPositions.size();
                z = suburbRoadPositions[roadIndex];
            }
            else {
                // Vertical suburban road
                z = -40.0f + ((rand() % 100) / 100.0f) * 80.0f;
                int roadIndex = rand() % suburbRoadPositions.size();
                x = suburbRoadPositions[roadIndex];
            }
        }
        else {
            // Fallback to highway
            x = 25.0f;
            z = ((rand() % 100) / 100.0f - 0.5f) * 80.0f;
        }
    }

    return glm::vec3(x, -0.8f, z);
}

glm::vec3 CityManager::getRandomPositionInField() {
    float x, z;
    int maxAttempts = 20;
    int attempts = 0;

    do {
        x = -35.0f + ((rand() % 100) / 100.0f) * 70.0f;
        z = -35.0f + ((rand() % 100) / 100.0f) * 70.0f;
        attempts++;
    } while ((isRoadArea(x, z) || isBuildingArea(x, z)) && attempts < maxAttempts);

    return glm::vec3(x, -0.9f, z);
}

bool CityManager::isPositionValid(const glm::vec3& pos, float radius) {
    // Relaxed validation - check city boundaries
    const float cityLimit = 45.0f; // More lenient boundary
    if (fabsf(pos.x) > cityLimit || fabsf(pos.z) > cityLimit) {
        return false;
    }

    // Simplified collision check - only check against very close entities
    for (const auto& entity : entities) {
        if (entity->type == EntityType::LAMP_POST || entity->type == EntityType::TRASH_BIN) {
            continue; // Skip furniture for less strict validation
        }

        glm::vec3 entityPos = glm::vec3(entity->localTransform[3]);
        float distance = glm::distance(pos, entityPos);
        if (distance < radius * 0.5f) { // Reduced collision radius
            return false;
        }
    }

    return true;
}

bool CityManager::isRoadArea(float x, float z) {
    const float tolerance = 0.5f; // Increased tolerance

    // Check major highways
    if ((fabsf(x - 25.0f) <= 4.0f && fabsf(z) <= 52.0f) || // North-South highway
        (fabsf(z + 25.0f) <= 4.0f && fabsf(x) <= 52.0f) || // East-West highway
        (fabsf(x - 25.0f) <= 6.0f && fabsf(z + 25.0f) <= 6.0f)) { // Highway interchange
        return true;
    }

    // Check downtown streets
    const float downtownSize = 18.0f;
    const float streetWidth = 3.0f;
    const float blockSize = 6.0f;

    if (fabsf(x) <= downtownSize && fabsf(z) <= downtownSize) {
        // Check horizontal streets
        for (float roadZ = -downtownSize; roadZ <= downtownSize; roadZ += blockSize) {
            if (fabsf(z - roadZ) <= (streetWidth / 2.0f + tolerance)) return true;
        }
        // Check vertical streets
        for (float roadX = -downtownSize; roadX <= downtownSize; roadX += blockSize) {
            if (fabsf(x - roadX) <= (streetWidth / 2.0f + tolerance)) return true;
        }
    }

    // Check suburban roads
    const float suburbSize = 45.0f;
    const float suburbBlockSize = 13.0f;

    // Suburban horizontal roads
    for (float roadZ = -suburbSize; roadZ <= suburbSize; roadZ += suburbBlockSize) {
        if (fabsf(roadZ) > downtownSize &&
            fabsf(z - roadZ) <= (streetWidth / 2.0f + tolerance) &&
            fabsf(x) <= suburbSize) {
            return true;
        }
    }
    // Suburban vertical roads
    for (float roadX = -suburbSize; roadX <= suburbSize; roadX += suburbBlockSize) {
        if (fabsf(roadX) > downtownSize &&
            fabsf(x - roadX) <= (streetWidth / 2.0f + tolerance) &&
            fabsf(z) <= suburbSize) {
            return true;
        }
    }

    // Check coastal road
    if (fabsf(z - 35.0f) <= 3.0f && fabsf(x) <= 32.0f) return true;

    return false;
}

bool CityManager::isBuildingArea(float x, float z) {
    // Simplified building area check with larger buffer zones
    if (fabsf(x) < 20.0f && fabsf(z) < 20.0f) {
        // Downtown core - check against major building positions
        std::vector<glm::vec3> majorBuildings = {
            {0, 0, 0}, {8, 0, 0}, {-8, 0, 0}, {0, 0, 8}, {0, 0, -8},
            {15, 0, 0}, {-15, 0, 0}, {0, 0, 15}, {0, 0, -15}
        };

        for (const auto& building : majorBuildings) {
            float distance = sqrtf(powf(x - building.x, 2.0f) + powf(z - building.z, 2.0f));
            if (distance < 4.0f) { // Larger buffer
                return true;
            }
        }
    }

    return false;
}

void CityManager::update() {
    cityRoot.update(glm::mat4(1.0f));

    // Update all entities with improved boundary checking
    for (auto& entity : entities) {
        if (entity->type == EntityType::TREE || entity->type == EntityType::GRASS_PATCH ||
            entity->type == EntityType::LAMP_POST || entity->type == EntityType::TRASH_BIN) {
            // Static entities don't need movement updates
            entity->update(cityRoot.worldTransform);
            continue;
        }

        // Store old position before update
        glm::vec3 oldPos = glm::vec3(entity->localTransform[3]);

        entity->update(cityRoot.worldTransform);

        // Check boundaries after update
        glm::vec3 newPos = glm::vec3(entity->localTransform[3]);
        const float cityLimit = 45.0f; // Keep entities well inside walls

        bool needsBoundaryCorrection = false;
        if (fabsf(newPos.x) > cityLimit) {
            newPos.x = (newPos.x > 0) ? cityLimit - 1.0f : -cityLimit + 1.0f;
            needsBoundaryCorrection = true;
        }
        if (fabsf(newPos.z) > cityLimit) {
            newPos.z = (newPos.z > 0) ? cityLimit - 1.0f : -cityLimit + 1.0f;
            needsBoundaryCorrection = true;
        }

        if (needsBoundaryCorrection) {
            // Reset entity position
            entity->localTransform = glm::mat4(1.0f);

            if (entity->type == EntityType::HUMAN) {
                newPos.y = -0.9f;
                entity->localTransform = glm::translate(entity->localTransform, newPos);
                entity->localTransform = glm::scale(entity->localTransform, glm::vec3(0.4f, 1.8f, 0.4f));
            }
            else if (entity->type == EntityType::CAR) {
                newPos.y = -0.8f;
                entity->localTransform = glm::translate(entity->localTransform, newPos);
                entity->localTransform = glm::scale(entity->localTransform, glm::vec3(2.0f, 1.0f, 4.0f));
            }

            // Stop movement and find new target
            entity->isMoving = false;
            entity->velocity = glm::vec3(0.0f);
        }
    }
}