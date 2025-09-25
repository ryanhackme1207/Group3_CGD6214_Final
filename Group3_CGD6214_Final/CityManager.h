// CityManager.h
#pragma once
#include "Entity.h"
#include <vector>
#include <random>

struct Road {
    float x, y, z;
    float length, width;
    bool isHorizontal; // 添加方向标识
};

struct Building {
    float x, y, z;
    float width, depth, height;
};

class CityManager {
private:
    std::vector<Road> roads;
    std::vector<Building> buildings;
    std::vector<Entity> entities;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;

public:
    CityManager(int seed = 0);
    void generateCity(int gridSize, float spacing);
    void spawnEntities(int numHumans, int numCars, int numTrees, int numFurniture);
    void update(float deltaTime);

    const std::vector<Entity>& getEntities() const { return entities; }
    const std::vector<Building>& getBuildings() const { return buildings; }
    const std::vector<Road>& getRoads() const { return roads; }

private:
    void generateRoads(int gridSize, float spacing);
    void generateBuildings(int gridSize, float spacing);
};