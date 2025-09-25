#pragma once

#include "Node.h"
#include "Building.h"
#include "Entity.h"
#include <vector>
#include <memory>
#include <glm/glm.hpp>

class CityManager {
public:
    CityManager();
    ~CityManager();

    void initializeCity();
    void update();

    // Public members for rendering
    Node cityRoot;
    std::vector<std::unique_ptr<Building>> buildings;
    std::vector<std::unique_ptr<Entity>> entities;

private:
    // Building creation methods
    void createBoundaryWalls();           // New method for creating boundary walls
    void createBuildings();
    void createDowntownDistrict();
    void createResidentialAreas();
    void createIndustrialZone();
             // Removed for smaller map

    // Road creation methods
    void createGridBasedRoads();
    void createHighwaySystem();
    void createCityStreets();
    void createStreetLights();
         // Removed for smaller map

    // Terrain methods
    void fillLandWithGrass();
    void createParks();
    void createNaturalFeatures();

    // Entity spawning methods
    void spawnHumans(int count);
    void spawnCars(int count);
    void spawnTrees(int count);          
    void spawnStreetFurniture(int count);

    // Position generation methods
    glm::vec3 getRandomPositionOnSidewalk();
    glm::vec3 getRandomPositionOnRoad();
    glm::vec3 getRandomPositionInField();

    // Validation and constraint methods
    bool isPositionValid(const glm::vec3& pos, float radius);
    bool isRoadArea(float x, float z);
    bool isBuildingArea(float x, float z);
};