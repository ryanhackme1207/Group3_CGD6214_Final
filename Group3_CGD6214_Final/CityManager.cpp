#include "CityManager.h"
#include <cmath>
#include <iostream>
#include <random>

CityManager::CityManager(int seed)
    : rng(seed), dist(0.0f, 1.0f) {
}

void CityManager::generateCity(int gridSize, float spacing) {
    buildings.clear();
    roads.clear();
    entities.clear();

    generateRoads(gridSize, spacing);
    generateBuildings(gridSize, spacing);
}

void CityManager::generateRoads(int gridSize, float spacing) {
    float roadWidth = 4.0f; // 扩大马路宽度

    // 横向道路 (X轴方向)
    for (int i = 0; i <= gridSize; ++i) {
        Road r;
        r.x = 0;
        r.y = 0.01f; // 稍微抬高避免z-fighting
        r.z = i * spacing;
        r.length = gridSize * spacing; // 道路长度
        r.width = roadWidth;
        r.isHorizontal = true;
        roads.push_back(r);
    }

    // 纵向道路 (Z轴方向)
    for (int j = 0; j <= gridSize; ++j) {
        Road r;
        r.x = j * spacing;
        r.y = 0.01f; // 稍微抬高避免z-fighting
        r.z = 0;
        r.length = gridSize * spacing;
        r.width = roadWidth;
        r.isHorizontal = false;
        roads.push_back(r);
    }

    // 添加一些对角线道路增加多样性
    for (int i = 0; i < gridSize / 2; ++i) {
        Road r;
        r.x = i * spacing * 2;
        r.y = 0.01f;
        r.z = 0;
        r.length = gridSize * spacing / 2;
        r.width = roadWidth * 0.7f;
        r.isHorizontal = false;
        roads.push_back(r);
    }
}

void CityManager::generateBuildings(int gridSize, float spacing) {
    float roadWidth = 4.0f;
    float buildingMargin = 1.0f; // 建筑物离马路的距离

    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            // 检查是否在道路交叉口，如果是则跳过建筑物生成
            bool isIntersection = (i % 2 == 0 && j % 2 == 0);
            if (isIntersection || dist(rng) < 0.2f) continue; // 留出更多空地

            // 计算建筑物位置，避开道路
            float posX = i * spacing + spacing / 2;
            float posZ = j * spacing + spacing / 2;

            // 确保建筑物不会建在道路上
            bool onRoad = false;
            for (const auto& road : roads) {
                if (road.isHorizontal) {
                    if (std::abs(posZ - road.z) < roadWidth / 2 + buildingMargin) {
                        onRoad = true;
                        break;
                    }
                }
                else {
                    if (std::abs(posX - road.x) < roadWidth / 2 + buildingMargin) {
                        onRoad = true;
                        break;
                    }
                }
            }

            if (onRoad) continue;

            Building b;
            b.x = posX;
            b.y = 0;
            b.z = posZ;
            b.width = 3 + dist(rng) * 4;  
            b.depth = 3 + dist(rng) * 4;  
            b.height = 8 + dist(rng) * 20; 
            buildings.push_back(b);
        }
    }
}

void CityManager::spawnEntities(int numHumans, int numCars, int numTrees, int numFurniture) {
    auto spawn = [&](int count, EntityType type, float minX, float maxX, float minZ, float maxZ) {
        for (int i = 0; i < count; ++i) {
            glm::vec3 position(
                minX + dist(rng) * (maxX - minX),
                0.0f,
                minZ + dist(rng) * (maxZ - minZ)
            );
            glm::vec3 scale(1.0f);

            if (type == EntityType::CAR) {
                scale = glm::vec3(1.5f, 0.8f, 3.0f);
            }
            else if (type == EntityType::TREE) {
                scale = glm::vec3(1.2f, 2.5f, 1.2f);
            }
            else if (type == EntityType::HUMAN) {
                scale = glm::vec3(0.4f);
            }
            else if (type == EntityType::LAMP_POST) {
                scale = glm::vec3(0.2f, 2.0f, 0.2f);
            }
            else if (type == EntityType::TRASH_BIN) {
                scale = glm::vec3(0.6f, 0.8f, 0.6f);
            }

            Entity entity(position, scale, type);
            entities.push_back(entity);
        }
        };

    float citySize = 100.0f;
    spawn(numHumans, EntityType::HUMAN, 0, citySize, 0, citySize);
    spawn(numCars, EntityType::CAR, 0, citySize, 0, citySize);
    spawn(numTrees, EntityType::TREE, 0, citySize, 0, citySize);
    spawn(numFurniture, EntityType::LAMP_POST, 0, citySize, 0, citySize);
    spawn(5, EntityType::TRASH_BIN, 0, citySize, 0, citySize); // 添加垃圾桶
}

void CityManager::update(float deltaTime) {
    for (auto& entity : entities) {
        entity.update(glm::mat4(1.0f));
    }
}