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
    createBuildings();
    createGridBasedRoads();
    fillLandWithGrass();
    spawnTrees(50);
    spawnHumans(25);
    spawnCars(25); // Increase car count
    spawnStreetFurniture(20);
}

void CityManager::createBuildings() {
    // Large ground plane to prevent seeing outside world - lower it so buildings sit properly
    auto ground = std::make_unique<Building>(glm::vec3(0, -1.5f, 0), glm::vec3(200, 0.2, 200), BuildingType::FIELD);
    cityRoot.addChild(ground.get());
    buildings.push_back(std::move(ground));

    // GTA V inspired city layout
    createDowntownDistrict();
    createResidentialAreas();
    createIndustrialZone();
    createBeachArea();
    createAirport();
}

void CityManager::createDowntownDistrict() {
    // Downtown skyscrapers (inspired by Downtown Los Santos)
    std::vector<std::tuple<glm::vec3, glm::vec3, BuildingType>> downtownBuildings = {
        // Central business district - main skyscrapers
        {{0, 0, 0}, {8, 25, 8}, BuildingType::SKYSCRAPER},
        {{12, 0, 0}, {10, 30, 10}, BuildingType::SKYSCRAPER},
        {{-12, 0, 0}, {9, 28, 9}, BuildingType::SKYSCRAPER},
        {{0, 0, 12}, {7, 22, 7}, BuildingType::SKYSCRAPER},
        {{0, 0, -12}, {11, 32, 11}, BuildingType::SKYSCRAPER},

        // Secondary downtown buildings
        {{15, 0, 15}, {6, 18, 6}, BuildingType::SHOP},
        {{-15, 0, 15}, {6, 20, 6}, BuildingType::SHOP},
        {{15, 0, -15}, {6, 16, 6}, BuildingType::SHOP},
        {{-15, 0, -15}, {7, 19, 7}, BuildingType::SHOP},

        // Additional downtown buildings - more density
        {{20, 0, 0}, {7, 26, 7}, BuildingType::SKYSCRAPER},
        {{-20, 0, 0}, {8, 24, 8}, BuildingType::SKYSCRAPER},
        {{0, 0, 20}, {6, 20, 6}, BuildingType::SHOP},
        {{0, 0, -20}, {8, 27, 8}, BuildingType::SKYSCRAPER},

        // Mid-rise buildings
        {{8, 0, 8}, {5, 15, 5}, BuildingType::SHOP},
        {{-8, 0, 8}, {5, 16, 5}, BuildingType::SHOP},
        {{8, 0, -8}, {5, 14, 5}, BuildingType::SHOP},
        {{-8, 0, -8}, {5, 17, 5}, BuildingType::SHOP},

        // Commercial buildings
        {{24, 0, 12}, {4, 12, 4}, BuildingType::HOUSE},
        {{-24, 0, 12}, {4, 13, 4}, BuildingType::HOUSE},
        {{24, 0, -12}, {4, 11, 4}, BuildingType::HOUSE},
        {{-24, 0, -12}, {4, 14, 4}, BuildingType::HOUSE},

        // More office buildings
        {{12, 0, 24}, {6, 18, 6}, BuildingType::SHOP},
        {{-12, 0, 24}, {6, 19, 6}, BuildingType::SHOP},
        {{12, 0, -24}, {6, 17, 6}, BuildingType::SHOP},
        {{-12, 0, -24}, {6, 20, 6}, BuildingType::SHOP}
    };

    for (const auto& buildingData : downtownBuildings) {
        auto building = std::make_unique<Building>(std::get<0>(buildingData), std::get<1>(buildingData), std::get<2>(buildingData));
        cityRoot.addChild(building.get());
        buildings.push_back(std::move(building));
    }
}

void CityManager::createResidentialAreas() {
    // Residential neighborhoods (inspired by Vinewood Hills, Rockford Hills)
    // Expand residential grid for more density
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            float x = -60.0f + i * 15.0f;
            float z = -60.0f + j * 15.0f;

            // Skip downtown area
            if (fabsf(x) < 30.0f && fabsf(z) < 30.0f) continue;

            // Skip airport area
            if (x > 50.0f) continue;

            // Create diverse residential buildings
            float height = 3.0f + (rand() % 8);
            glm::vec3 scale = glm::vec3(4 + (rand() % 4), height, 4 + (rand() % 4));

            // Vary building types in residential areas
            BuildingType type;
            int typeRand = rand() % 100;
            if (typeRand < 70) {
                type = BuildingType::HOUSE;
            }
            else if (typeRand < 90) {
                type = BuildingType::SHOP;
                scale.y = height * 0.8f; // Shops are shorter
            }
            else {
                type = BuildingType::SKYSCRAPER;
                scale.y = height * 1.5f; // Some apartment buildings
            }

            auto building = std::make_unique<Building>(glm::vec3(x, 0, z), scale, type);
            cityRoot.addChild(building.get());
            buildings.push_back(std::move(building));

            // Add smaller buildings around main ones
            if (rand() % 100 < 30) {
                float offsetX = ((rand() % 100) - 50) / 10.0f;
                float offsetZ = ((rand() % 100) - 50) / 10.0f;
                float smallHeight = 2.0f + (rand() % 4);
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
    // Industrial area (inspired by Port of Los Santos)
    for (int i = 0; i < 3; ++i) {
        float x = -60.0f + i * 15.0f;
        float z = -60.0f;

        float height = 8.0f + (rand() % 5);
        glm::vec3 scale = glm::vec3(12, height, 8);

        auto warehouse = std::make_unique<Building>(glm::vec3(x, 0, z), scale, BuildingType::SHOP);
        cityRoot.addChild(warehouse.get());
        buildings.push_back(std::move(warehouse));
    }
}

void CityManager::createBeachArea() {
    // Beach boardwalk buildings (inspired by Vespucci Beach)
    for (int i = 0; i < 5; ++i) {
        float x = -30.0f + i * 15.0f;
        float z = 45.0f;

        float height = 2.0f + (rand() % 4);
        glm::vec3 scale = glm::vec3(4, height, 3);

        auto beachBuilding = std::make_unique<Building>(glm::vec3(x, 0, z), scale, BuildingType::HOUSE);
        cityRoot.addChild(beachBuilding.get());
        buildings.push_back(std::move(beachBuilding));
    }
}

void CityManager::createAirport() {
    // Airport terminals (inspired by Los Santos International)
    auto terminal1 = std::make_unique<Building>(glm::vec3(60, 0, 0), glm::vec3(20, 6, 40), BuildingType::SHOP);
    cityRoot.addChild(terminal1.get());
    buildings.push_back(std::move(terminal1));

    auto terminal2 = std::make_unique<Building>(glm::vec3(60, 0, -25), glm::vec3(15, 5, 30), BuildingType::SHOP);
    cityRoot.addChild(terminal2.get());
    buildings.push_back(std::move(terminal2));
}

void CityManager::createGridBasedRoads() {
    createHighwaySystem();
    createCityStreets();
    createAirportRunways();
}

void CityManager::createHighwaySystem() {
    // Major highways (inspired by GTA V freeway system)

    // Interstate equivalent - main highway running North-South
    auto highway_ns = std::make_unique<Building>(
        glm::vec3(35, -1.35f, 0),
        glm::vec3(8, 0.1f, 160), // Wide highway
        BuildingType::ROAD
    );
    cityRoot.addChild(highway_ns.get());
    buildings.push_back(std::move(highway_ns));

    // Interstate equivalent - main highway running East-West
    auto highway_ew = std::make_unique<Building>(
        glm::vec3(0, -1.35f, -35),
        glm::vec3(160, 0.1f, 8), // Wide highway
        BuildingType::ROAD
    );
    cityRoot.addChild(highway_ew.get());
    buildings.push_back(std::move(highway_ew));

    // Highway interchange
    auto interchange = std::make_unique<Building>(
        glm::vec3(35, -1.35f, -35),
        glm::vec3(12, 0.1f, 12),
        BuildingType::ROAD
    );
    cityRoot.addChild(interchange.get());
    buildings.push_back(std::move(interchange));
}

void CityManager::createCityStreets() {
    // Downtown street grid (denser)
    const float downtownSize = 25.0f;
    const float streetWidth = 4.0f;
    const float blockSize = 8.0f;

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

    // Suburban roads (wider spacing)
    const float suburbSize = 80.0f;
    const float suburbBlockSize = 15.0f;

    // Suburban horizontal roads
    for (float z = -suburbSize; z <= suburbSize; z += suburbBlockSize) {
        if (fabsf(z) > downtownSize) { // Don't overlap with downtown
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
        if (fabsf(x) > downtownSize) { // Don't overlap with downtown
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
        glm::vec3(0, -1.35f, 50),
        glm::vec3(80, 0.1f, 6),
        BuildingType::ROAD
    );
    cityRoot.addChild(coastalRoad.get());
    buildings.push_back(std::move(coastalRoad));

    // Add street lights after creating roads
    createStreetLights();
}

void CityManager::createStreetLights() {
    // Add street lights along major roads
    const float lightSpacing = 12.0f; // Distance between lights

    // Downtown street lights (denser)
    const float downtownSize = 25.0f;
    const float blockSize = 8.0f;

    // Lights along downtown horizontal streets
    for (float z = -downtownSize; z <= downtownSize; z += blockSize) {
        for (float x = -downtownSize; x <= downtownSize; x += lightSpacing) {
            // Skip if too close to buildings
            if (isBuildingArea(x, z)) continue;

            // Create street light on both sides of the road
            auto light1 = std::make_unique<Entity>(
                glm::vec3(x, -1.25f, z + 3.0f),
                glm::vec3(0.3f, 4.0f, 0.3f),
                EntityType::LAMP_POST
            );
            cityRoot.addChild(light1.get());
            entities.push_back(std::move(light1));

            auto light2 = std::make_unique<Entity>(
                glm::vec3(x, -1.25f, z - 3.0f),
                glm::vec3(0.3f, 4.0f, 0.3f),
                EntityType::LAMP_POST
            );
            cityRoot.addChild(light2.get());
            entities.push_back(std::move(light2));
        }
    }

    // Lights along downtown vertical streets
    for (float x = -downtownSize; x <= downtownSize; x += blockSize) {
        for (float z = -downtownSize; z <= downtownSize; z += lightSpacing) {
            // Skip if too close to buildings
            if (isBuildingArea(x, z)) continue;

            // Create street light on both sides of the road
            auto light1 = std::make_unique<Entity>(
                glm::vec3(x + 3.0f, -1.25f, z),
                glm::vec3(0.3f, 4.0f, 0.3f),
                EntityType::LAMP_POST
            );
            cityRoot.addChild(light1.get());
            entities.push_back(std::move(light1));

            auto light2 = std::make_unique<Entity>(
                glm::vec3(x - 3.0f, -1.25f, z),
                glm::vec3(0.3f, 4.0f, 0.3f),
                EntityType::LAMP_POST
            );
            cityRoot.addChild(light2.get());
            entities.push_back(std::move(light2));
        }
    }

    // Highway street lights (wider spacing)
    for (float z = -80.0f; z <= 80.0f; z += lightSpacing * 2.0f) {
        // North-South highway lights
        auto highway_light1 = std::make_unique<Entity>(
            glm::vec3(40.0f, -1.25f, z),
            glm::vec3(0.4f, 5.0f, 0.4f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(highway_light1.get());
        entities.push_back(std::move(highway_light1));

        auto highway_light2 = std::make_unique<Entity>(
            glm::vec3(30.0f, -1.25f, z),
            glm::vec3(0.4f, 5.0f, 0.4f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(highway_light2.get());
        entities.push_back(std::move(highway_light2));
    }

    // East-West highway lights
    for (float x = -80.0f; x <= 80.0f; x += lightSpacing * 2.0f) {
        auto highway_light3 = std::make_unique<Entity>(
            glm::vec3(x, -1.25f, -30.0f),
            glm::vec3(0.4f, 5.0f, 0.4f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(highway_light3.get());
        entities.push_back(std::move(highway_light3));

        auto highway_light4 = std::make_unique<Entity>(
            glm::vec3(x, -1.25f, -40.0f),
            glm::vec3(0.4f, 5.0f, 0.4f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(highway_light4.get());
        entities.push_back(std::move(highway_light4));
    }

    // Coastal road lights
    for (float x = -40.0f; x <= 40.0f; x += lightSpacing) {
        auto beach_light1 = std::make_unique<Entity>(
            glm::vec3(x, -1.25f, 47.0f),
            glm::vec3(0.3f, 3.5f, 0.3f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(beach_light1.get());
        entities.push_back(std::move(beach_light1));

        auto beach_light2 = std::make_unique<Entity>(
            glm::vec3(x, -1.25f, 53.0f),
            glm::vec3(0.3f, 3.5f, 0.3f),
            EntityType::LAMP_POST
        );
        cityRoot.addChild(beach_light2.get());
        entities.push_back(std::move(beach_light2));
    }
}

void CityManager::createAirportRunways() {
    // Airport runways (inspired by LSIA)
    auto runway1 = std::make_unique<Building>(
        glm::vec3(70, -1.4f, 0),
        glm::vec3(10, 0.05f, 80),
        BuildingType::ROAD
    );
    cityRoot.addChild(runway1.get());
    buildings.push_back(std::move(runway1));

    auto runway2 = std::make_unique<Building>(
        glm::vec3(85, -1.4f, 15),
        glm::vec3(10, 0.05f, 60),
        BuildingType::ROAD
    );
    cityRoot.addChild(runway2.get());
    buildings.push_back(std::move(runway2));

    // Taxiways
    auto taxiway = std::make_unique<Building>(
        glm::vec3(77, -1.4f, 0),
        glm::vec3(20, 0.05f, 4),
        BuildingType::ROAD
    );
    cityRoot.addChild(taxiway.get());
    buildings.push_back(std::move(taxiway));
}

void CityManager::fillLandWithGrass() {
    // Create seamless terrain coverage inspired by GTA V's natural landscapes
    const float terrainSize = 120.0f; // Expanded coverage area
    const float grassTileSize = 3.0f; // Smaller tiles for better coverage

    // First pass: Create base terrain everywhere (no skipping)
    for (float x = -terrainSize; x <= terrainSize; x += grassTileSize) {
        for (float z = -terrainSize; z <= terrainSize; z += grassTileSize) {
            // Create overlapping terrain tiles for seamless coverage
            glm::vec3 terrainPos = glm::vec3(x, -1.46f, z); // Slightly lower than roads

            // Vary terrain type based on location
            BuildingType terrainType = BuildingType::FIELD;
            glm::vec3 tileSize = glm::vec3(grassTileSize * 1.3f, 0.02f, grassTileSize * 1.3f);

            // Beach/coastal areas
            if (z > 45.0f) {
                tileSize.y = 0.01f; // Flatter for beach
            }
            // Airport areas
            else if (x > 55.0f && x < 100.0f && z > -50.0f && z < 50.0f) {
                tileSize.y = 0.005f; // Very flat for airport
            }
            // Downtown areas - more concrete/urban
            else if (fabsf(x) < 35.0f && fabsf(z) < 35.0f) {
                tileSize.y = 0.015f; // Slightly different for urban feel
            }

            auto terrainTile = std::make_unique<Building>(terrainPos, tileSize, terrainType);
            cityRoot.addChild(terrainTile.get());
            buildings.push_back(std::move(terrainTile));
        }
    }

    // Second pass: Add additional grass patches to fill gaps
    const float smallTileSize = 2.0f;
    for (float x = -terrainSize; x <= terrainSize; x += smallTileSize) {
        for (float z = -terrainSize; z <= terrainSize; z += smallTileSize) {
            // Only add if not on roads
            if (isRoadArea(x, z)) continue;

            glm::vec3 terrainPos = glm::vec3(x + 1.0f, -1.45f, z + 1.0f); // Offset position
            glm::vec3 tileSize = glm::vec3(smallTileSize * 1.1f, 0.015f, smallTileSize * 1.1f);

            auto fillTile = std::make_unique<Building>(terrainPos, tileSize, BuildingType::FIELD);
            cityRoot.addChild(fillTile.get());
            buildings.push_back(std::move(fillTile));
        }
    }

    // Add natural features - parks, green spaces (inspired by GTA V parks)
    createParks();
    createNaturalFeatures();
}

void CityManager::createParks() {
    // Central Park equivalent (Pershing Square area)
    for (int i = 0; i < 20; ++i) {
        float x = -8.0f + ((rand() % 100) / 100.0f) * 16.0f;
        float z = 8.0f + ((rand() % 100) / 100.0f) * 8.0f;

        float size = 1.2f + (rand() % 60) / 100.0f;
        auto parkGrass = std::make_unique<Entity>(
            glm::vec3(x, -0.82f, z),
            glm::vec3(size, 0.3f, size),
            EntityType::GRASS_PATCH
        );
        cityRoot.addChild(parkGrass.get());
        entities.push_back(std::move(parkGrass));
    }

    // Suburban parks
    std::vector<glm::vec2> parkCenters = { {-45.0f, -45.0f}, {45.0f, 45.0f}, {-45.0f, 45.0f}, {45.0f, -45.0f} };

    for (const auto& center : parkCenters) {
        for (int i = 0; i < 15; ++i) {
            float x = center.x + ((rand() % 100) / 100.0f - 0.5f) * 20.0f;
            float z = center.y + ((rand() % 100) / 100.0f - 0.5f) * 20.0f;

            if (isRoadArea(x, z) || isBuildingArea(x, z)) continue;

            float size = 1.5f + (rand() % 50) / 100.0f;
            auto parkFeature = std::make_unique<Entity>(
                glm::vec3(x, -0.82f, z),
                glm::vec3(size, 0.25f, size),
                EntityType::GRASS_PATCH
            );
            cityRoot.addChild(parkFeature.get());
            entities.push_back(std::move(parkFeature));
        }
    }
}

void CityManager::createNaturalFeatures() {
    // Beach vegetation
    for (int i = 0; i < 30; ++i) {
        float x = -60.0f + ((rand() % 100) / 100.0f) * 120.0f;
        float z = 48.0f + ((rand() % 100) / 100.0f) * 8.0f;

        if (isRoadArea(x, z)) continue;

        float size = 0.8f + (rand() % 40) / 100.0f;
        auto beachGrass = std::make_unique<Entity>(
            glm::vec3(x, -0.83f, z),
            glm::vec3(size, 0.15f, size),
            EntityType::GRASS_PATCH
        );
        cityRoot.addChild(beachGrass.get());
        entities.push_back(std::move(beachGrass));
    }

    // Industrial area sparse vegetation
    for (int i = 0; i < 10; ++i) {
        float x = -70.0f + ((rand() % 100) / 100.0f) * 30.0f;
        float z = -70.0f + ((rand() % 100) / 100.0f) * 20.0f;

        if (isRoadArea(x, z) || isBuildingArea(x, z)) continue;

        float size = 0.6f + (rand() % 30) / 100.0f;
        auto industrialGrass = std::make_unique<Entity>(
            glm::vec3(x, -0.84f, z),
            glm::vec3(size, 0.12f, size),
            EntityType::GRASS_PATCH
        );
        cityRoot.addChild(industrialGrass.get());
        entities.push_back(std::move(industrialGrass));
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
    float x = (rand() % 5000 - 2500) / 100.0f; // -25 to 25
    float z = (rand() % 5000 - 2500) / 100.0f; // -25 to 25

    // Avoid road areas (simplified)
    if (fabsf(x) < 2.0f || fabsf(x - 10.0f) < 2.0f || fabsf(x + 10.0f) < 2.0f ||
        fabsf(z) < 2.0f || fabsf(z - 10.0f) < 2.0f || fabsf(z + 10.0f) < 2.0f) {
        // Offset to sidewalk
        if (fabsf(x) < fabsf(z)) {
            x += (x > 0) ? 3 : -3;
        }
        else {
            z += (z > 0) ? 3 : -3;
        }
    }

    return glm::vec3(x, -0.9f, z); // Place humans on ground level
}

glm::vec3 CityManager::getRandomPositionOnRoad() {
    // Random position on the new grid-based roads
    const float blockSize = 10.0f;
    std::vector<float> roadPositions;

    // Build list of road positions (-30 to 30, every 10 units)
    for (float pos = -30.0f; pos <= 30.0f; pos += blockSize) {
        roadPositions.push_back(pos);
    }

    int roadChoice = rand() % 2; // 0 = horizontal, 1 = vertical
    float x, z;

    if (roadChoice == 0) {
        // Horizontal road
        x = -25.0f + ((rand() % 5000) / 100.0f); // Random X from -25 to 25
        int roadIndex = rand() % roadPositions.size();
        z = roadPositions[roadIndex] + ((rand() % 200 - 100) / 100.0f); // Stay on road with slight variation
    }
    else {
        // Vertical road
        z = -25.0f + ((rand() % 5000) / 100.0f); // Random Z from -25 to 25
        int roadIndex = rand() % roadPositions.size();
        x = roadPositions[roadIndex] + ((rand() % 200 - 100) / 100.0f); // Stay on road with slight variation
    }

    return glm::vec3(x, -0.8f, z);
}

glm::vec3 CityManager::getRandomPositionInField() {
    float x, z;
    do {
        x = (rand() % 5000 - 2500) / 100.0f; // -25 to 25
        z = (rand() % 5000 - 2500) / 100.0f; // -25 to 25
    } while (fabsf(x) < 3.0f && fabsf(z) < 3.0f); // Avoid center roads

    return glm::vec3(x, -0.9f, z); // Place on ground level
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

bool CityManager::isRoadArea(float x, float z) {
    const float tolerance = 0.1f; // Small tolerance for more precise detection

    // Check major highways - more precise boundaries
    if ((fabsf(x - 35.0f) <= 4.5f && fabsf(z) <= 85.0f) || // North-South highway (wider detection)
        (fabsf(z + 35.0f) <= 4.5f && fabsf(x) <= 85.0f) || // East-West highway (wider detection)
        (fabsf(x - 35.0f) <= 7.0f && fabsf(z + 35.0f) <= 7.0f)) { // Highway interchange (larger area)
        return true;
    }

    // Check downtown streets (denser grid) - more precise
    const float downtownSize = 25.0f;
    const float streetWidth = 4.0f;
    const float blockSize = 8.0f;

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

    // Check suburban roads - more precise boundaries
    const float suburbSize = 80.0f;
    const float suburbBlockSize = 15.0f;

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

    // Check coastal road - more precise
    if (fabsf(z - 50.0f) <= 3.5f && fabsf(x) <= 42.0f) return true;

    // Check airport runways - more precise
    if ((fabsf(x - 70.0f) <= 6.0f && fabsf(z) <= 42.0f) ||
        (fabsf(x - 85.0f) <= 6.0f && fabsf(z - 15.0f) <= 32.0f) ||
        (fabsf(z) <= 2.5f && x >= 63.0f && x <= 92.0f)) { // Taxiway (wider area)
        return true;
    }

    return false;
}

bool CityManager::isBuildingArea(float x, float z) {
    // Check against predefined building positions with some buffer
    std::vector<glm::vec3> buildingPositions = {
        {-20, 0, -20}, {-10, 0, -20}, {0, 0, -20}, {10, 0, -20}, {20, 0, -20},
        {-20, 0, -10}, {-10, 0, -10}, {10, 0, -10}, {20, 0, -10},
        {-20, 0, 0}, {20, 0, 0},
        {-20, 0, 10}, {-10, 0, 10}, {10, 0, 10}, {20, 0, 10},
        {-20, 0, 20}, {-10, 0, 20}, {0, 0, 20}, {10, 0, 20}, {20, 0, 20}
    };

    for (const auto& buildingPos : buildingPositions) {
        float distance = sqrtf(powf(x - buildingPos.x, 2.0f) + powf(z - buildingPos.z, 2.0f));
        if (distance < 4.0f) { // Building footprint + buffer
            return true;
        }
    }

    return false;
}

void CityManager::update() {
    cityRoot.update(glm::mat4(1.0f));  // update root with identity matrix


    // Update all entities with cityRoot�s world transform
    for (auto& entity : entities) {
        entity->update(cityRoot.worldTransform);
    }
}
