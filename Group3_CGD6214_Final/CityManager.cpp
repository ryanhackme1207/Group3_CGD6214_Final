#include "CityManager.h"
#include <iostream>
#include <algorithm>

CityManager::CityManager(int seed) : rng(seed), dist(0.0f, 1.0f) {
    rootNode = std::make_unique<Node>();
    infrastructureNode = std::make_unique<Node>();
    buildingsNode = std::make_unique<Node>();
    entitiesNode = std::make_unique<Node>();
    vegetationNode = std::make_unique<Node>();
    decorationsNode = std::make_unique<Node>();

    rootNode->addChild(infrastructureNode.get());
    infrastructureNode->addChild(buildingsNode.get());
    infrastructureNode->addChild(entitiesNode.get());
    entitiesNode->addChild(vegetationNode.get());
    entitiesNode->addChild(decorationsNode.get());

    spatialTree = std::make_unique<QuadTreeNode>(glm::vec2(0.0f, 0.0f), glm::vec2(100.0f, 100.0f));

    lodNearDistance = 50.0f;
    lodFarDistance = 150.0f;
    terrainParams = { 4, 0.5f, 0.1f, 10.0f };
    lastCameraPosition = glm::vec3(0.0f);
}

CityManager::~CityManager() = default;

void CityManager::generateComplexScene(int complexity) {
    roads.clear();
    buildings.clear();
    entities.clear();
    streetLights.clear();
    trafficSigns.clear();
    vegetation.clear();
    vehicles.clear();
    streetFurniture.clear();
    spatialTree->clear();

    int baseGridSize = 6 + complexity;
    float baseSpacing = 20.0f;

    generateAdvancedRoads(baseGridSize, baseSpacing);
    generateAdvancedBuildings(baseGridSize, baseSpacing);
    generateVegetation(25 + complexity * 10);
    generateStreetFurniture(15 + complexity * 5);
    generateTrafficSystem();
    spawnEntities(20, 15, 30, 20);
    updateSpatialPartitioning();

    std::cout << "Generated complex scene with " << getSceneStatistics().totalObjects << " objects" << std::endl;
}

void CityManager::generateAdvancedRoads(int gridSize, float spacing) {
    float roadWidth = 4.0f;

    for (int i = 0; i <= gridSize; ++i) {
        Road r;
        r.x = 0;
        r.y = 0.01f;
        r.z = i * spacing - gridSize * spacing / 2;
        r.length = gridSize * spacing;
        r.width = roadWidth;
        r.isHorizontal = true;
        r.lodLevel = 0;
        roads.push_back(r);

        r.x = i * spacing - gridSize * spacing / 2;
        r.z = 0;
        r.isHorizontal = false;
        roads.push_back(r);
    }
}

void CityManager::generateAdvancedBuildings(int gridSize, float spacing) {
    float roadWidth = 4.0f;
    float buildingMargin = 2.0f;

    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            if (dist(rng) < 0.2f) continue;

            glm::vec3 position(
                i * spacing - gridSize * spacing / 2 + spacing / 2,
                0,
                j * spacing - gridSize * spacing / 2 + spacing / 2
            );

            AdvancedBuilding building;
            building.position = position;

            float distFromCenter = glm::length(glm::vec2(position.x, position.z));

            if (distFromCenter < gridSize * spacing * 0.15f) {
                building.type = BuildingType::SKYSCRAPER;
                building.scale = glm::vec3(4 + dist(rng) * 3, 15 + dist(rng) * 25, 4 + dist(rng) * 3);
            }
            else if (distFromCenter < gridSize * spacing * 0.35f) {
                building.type = BuildingType::SHOP;
                building.scale = glm::vec3(3 + dist(rng) * 2, 6 + dist(rng) * 8, 3 + dist(rng) * 2);
            }
            else {
                building.type = BuildingType::HOUSE;
                building.scale = glm::vec3(2 + dist(rng) * 2, 4 + dist(rng) * 6, 2 + dist(rng) * 2);
            }

            building.floors = (int)(building.scale.y / 3.0f);
            building.lodDistance = glm::length(position);
            generateBuildingDetails(building);
            buildings.push_back(building);
        }
    }
}

void CityManager::generateBuildingDetails(AdvancedBuilding& building) {
    for (int floor = 0; floor < building.floors; ++floor) {
        float floorHeight = floor * 3.0f + 1.5f;

        for (int w = 0; w < (int)(building.scale.x / 2); ++w) {
            building.windows.push_back(glm::vec3(
                -building.scale.x / 2 + w * 2.0f + 1.0f,
                floorHeight,
                building.scale.z / 2 + 0.1f
            ));
        }
    }

    switch (building.type) {
    case BuildingType::SKYSCRAPER:
        building.color = glm::vec3(0.3f + dist(rng) * 0.3f, 0.3f + dist(rng) * 0.3f, 0.5f + dist(rng) * 0.3f);
        break;
    case BuildingType::SHOP:
        building.color = glm::vec3(0.5f + dist(rng) * 0.3f, 0.4f + dist(rng) * 0.3f, 0.2f + dist(rng) * 0.3f);
        break;
    case BuildingType::HOUSE:
        building.color = glm::vec3(0.6f + dist(rng) * 0.3f, 0.5f + dist(rng) * 0.3f, 0.3f + dist(rng) * 0.3f);
        break;
    default:
        building.color = glm::vec3(0.7f, 0.7f, 0.7f);
    }
}

void CityManager::generateVegetation(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 position(
            -50.0f + dist(rng) * 100.0f,
            0.0f,
            -50.0f + dist(rng) * 100.0f
        );

        auto tree = std::make_unique<Entity>(position, glm::vec3(1.2f, 2.5f + dist(rng) * 3.0f, 1.2f), EntityType::TREE);
        vegetation.push_back(std::move(tree));
    }
}

void CityManager::generateStreetFurniture(int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 position(
            dist(rng) * 80.0f - 40.0f,
            0.0f,
            dist(rng) * 80.0f - 40.0f
        );

        EntityType furnitureType = (dist(rng) > 0.5f) ? EntityType::LAMP_POST : EntityType::TRASH_BIN;
        glm::vec3 scale = (furnitureType == EntityType::LAMP_POST) ?
            glm::vec3(0.2f, 3.0f, 0.2f) : glm::vec3(0.6f, 1.0f, 0.6f);

        auto furniture = std::make_unique<Entity>(position, scale, furnitureType);
        streetFurniture.push_back(std::move(furniture));
    }
}

void CityManager::generateTrafficSystem() {
    for (size_t i = 0; i < roads.size() && i < 10; ++i) {
        glm::vec3 position(roads[i].x, 2.0f, roads[i].z);
        auto trafficLight = std::make_unique<Entity>(position, glm::vec3(0.3f, 4.0f, 0.3f), EntityType::LAMP_POST);
        trafficSigns.push_back(std::move(trafficLight));
    }
}

void CityManager::spawnEntities(int numHumans, int numCars, int numTrees, int numFurniture) {
    entities.clear();

    auto spawnEntityType = [&](int count, EntityType type) {
        for (int i = 0; i < count; ++i) {
            glm::vec3 position(
                -40.0f + dist(rng) * 80.0f,
                0.0f,
                -40.0f + dist(rng) * 80.0f
            );

            glm::vec3 scale(1.0f);
            switch (type) {
            case EntityType::HUMAN:
                scale = glm::vec3(0.4f, 1.8f, 0.3f);
                break;
            case EntityType::CAR:
                scale = glm::vec3(1.5f, 0.8f, 3.0f);
                break;
            case EntityType::TREE:
                scale = glm::vec3(1.2f, 2.5f, 1.2f);
                break;
            default:
                break;
            }

            auto entity = std::make_unique<Entity>(position, scale, type);
            entities.push_back(std::move(entity));
        }
        };

    spawnEntityType(numHumans, EntityType::HUMAN);
    spawnEntityType(numCars, EntityType::CAR);
}

void CityManager::updateLOD(const glm::vec3& cameraPosition) {
    lastCameraPosition = cameraPosition;
    for (auto& building : buildings) {
        building.lodDistance = glm::length(building.position - cameraPosition);
    }
}

void CityManager::updateSpatialPartitioning() {
    spatialTree->clear();
    for (const auto& entity : entities) {
        spatialTree->insert(entity.get());
    }
}

void CityManager::optimizeScene(const glm::vec3& cameraPosition, const glm::mat4& viewProjection) {
    updateLOD(cameraPosition);
    performFrustumCulling(viewProjection);
}

void CityManager::performFrustumCulling(const glm::mat4& viewProjection) {
    visibleEntities.clear();
    visibleBuildings.clear();

    for (const auto& entity : entities) {
        glm::vec3 entityPos = glm::vec3(entity->worldTransform[3]);
        float distance = glm::length(entityPos - lastCameraPosition);
        if (distance < lodFarDistance) {
            visibleEntities.push_back(entity.get());
        }
    }

    for (auto& building : buildings) {
        if (building.lodDistance < lodFarDistance) {
            visibleBuildings.push_back(&building);
        }
    }
}

void CityManager::update(const glm::mat4& parentTransform) {
    for (const auto& entity : entities) {
        entity->update(parentTransform);
    }
    for (const auto& tree : vegetation) {
        tree->update(parentTransform);
    }
    for (const auto& furniture : streetFurniture) {
        furniture->update(parentTransform);
    }
    rootNode->update(parentTransform);
    Node::update(parentTransform);
}

CityManager::SceneStats CityManager::getSceneStatistics() const {
    SceneStats stats;
    stats.totalObjects = roads.size() + buildings.size() + entities.size() +
        vegetation.size() + streetFurniture.size() + trafficSigns.size();
    stats.visibleObjects = visibleEntities.size() + visibleBuildings.size();
    stats.hierarchyLevels = 5;
    stats.lodNearCount = 0;
    stats.lodFarCount = 0;
    stats.frameTime = 0.0f;
    return stats;
}

// QuadTreeNode Implementation
QuadTreeNode::QuadTreeNode(glm::vec2 center, glm::vec2 halfDim)
    : center(center), halfDimension(halfDim), isLeaf(true) {
}

void QuadTreeNode::insert(Entity* entity) {
    if (!entity) return;

    glm::vec3 pos = glm::vec3(entity->worldTransform[3]);
    glm::vec2 entityPos(pos.x, pos.z);

    if (entityPos.x < center.x - halfDimension.x || entityPos.x > center.x + halfDimension.x ||
        entityPos.y < center.y - halfDimension.y || entityPos.y > center.y + halfDimension.y) {
        return;
    }

    if (isLeaf && entities.size() < MAX_ENTITIES_PER_NODE) {
        entities.push_back(entity);
    }
    else {
        if (isLeaf) {
            subdivide();
        }
        for (auto& child : children) {
            child->insert(entity);
        }
    }
}

void QuadTreeNode::subdivide() {
    isLeaf = false;
    glm::vec2 quarter = halfDimension * 0.5f;

    children.push_back(std::make_unique<QuadTreeNode>(
        glm::vec2(center.x - quarter.x, center.y - quarter.y), quarter));
    children.push_back(std::make_unique<QuadTreeNode>(
        glm::vec2(center.x + quarter.x, center.y - quarter.y), quarter));
    children.push_back(std::make_unique<QuadTreeNode>(
        glm::vec2(center.x - quarter.x, center.y + quarter.y), quarter));
    children.push_back(std::make_unique<QuadTreeNode>(
        glm::vec2(center.x + quarter.x, center.y + quarter.y), quarter));

    for (Entity* entity : entities) {
        for (auto& child : children) {
            child->insert(entity);
        }
    }
    entities.clear();
}

std::vector<Entity*> QuadTreeNode::query(glm::vec2 queryCenter, glm::vec2 queryHalfDim) {
    std::vector<Entity*> result;

    if (queryCenter.x - queryHalfDim.x > center.x + halfDimension.x ||
        queryCenter.x + queryHalfDim.x < center.x - halfDimension.x ||
        queryCenter.y - queryHalfDim.y > center.y + halfDimension.y ||
        queryCenter.y + queryHalfDim.y < center.y - halfDimension.y) {
        return result;
    }

    if (isLeaf) {
        result = entities;
    }
    else {
        for (auto& child : children) {
            auto childResult = child->query(queryCenter, queryHalfDim);
            result.insert(result.end(), childResult.begin(), childResult.end());
        }
    }
    return result;
}

void QuadTreeNode::clear() {
    entities.clear();
    children.clear();
    isLeaf = true;
}