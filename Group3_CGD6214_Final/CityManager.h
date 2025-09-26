#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include "Node.h"
#include "Entity.h"
#include "Building.h"

// Advanced Road structure with LOD support
struct Road {
    float x, y, z;
    float length, width;
    bool isHorizontal;
    int lodLevel; // Level of detail
    std::vector<glm::vec3> decorations; // Traffic lights, signs, etc.
};

// Advanced Building with hierarchy
struct AdvancedBuilding {
    glm::vec3 position;
    glm::vec3 scale;
    BuildingType type;
    int floors;
    float lodDistance;
    std::vector<glm::vec3> windows;
    std::vector<glm::vec3> details; // Architectural details
    glm::vec3 color;
};

// Scene Quadtree for spatial partitioning
class QuadTreeNode {
public:
    glm::vec2 center;
    glm::vec2 halfDimension;
    std::vector<Entity*> entities;
    std::vector<std::unique_ptr<QuadTreeNode>> children;
    bool isLeaf;
    static const int MAX_ENTITIES_PER_NODE = 10;

    QuadTreeNode(glm::vec2 center, glm::vec2 halfDim);
    void insert(Entity* entity);
    void subdivide();
    std::vector<Entity*> query(glm::vec2 queryCenter, glm::vec2 queryHalfDim);
    void clear();
};

// Enhanced CityManager with advanced scene management
class CityManager : public Node {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;

    // Core city data with hierarchy levels (4+ levels)
    std::unique_ptr<Node> rootNode;           // Level 1: Root
    std::unique_ptr<Node> infrastructureNode; // Level 2: Infrastructure
    std::unique_ptr<Node> buildingsNode;      // Level 3: Buildings  
    std::unique_ptr<Node> entitiesNode;       // Level 4: Dynamic entities
    std::unique_ptr<Node> vegetationNode;     // Level 4: Trees, grass
    std::unique_ptr<Node> decorationsNode;    // Level 4: Street furniture

    // 50+ distinct objects requirement
    std::vector<Road> roads;
    std::vector<AdvancedBuilding> buildings;
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<Entity>> streetLights;
    std::vector<std::unique_ptr<Entity>> trafficSigns;
    std::vector<std::unique_ptr<Entity>> vegetation;
    std::vector<std::unique_ptr<Entity>> vehicles;
    std::vector<std::unique_ptr<Entity>> streetFurniture;

    // Spatial partitioning for performance
    std::unique_ptr<QuadTreeNode> spatialTree;

    // LOD system variables
    float lodNearDistance;
    float lodFarDistance;
    glm::vec3 lastCameraPosition;

    // Procedural generation parameters
    struct TerrainParams {
        int octaves;
        float persistence;
        float scale;
        float heightMultiplier;
    } terrainParams;

    // Performance optimization
    std::vector<Entity*> visibleEntities;
    std::vector<AdvancedBuilding*> visibleBuildings;

    // Generation methods
    void generateAdvancedRoads(int gridSize, float spacing);
    void generateAdvancedBuildings(int gridSize, float spacing);
    void generateProceduralTerrain(int width, int height);
    void generateVegetation(int count);
    void generateStreetFurniture(int count);
    void generateTrafficSystem();

    // LOD and optimization methods
    void updateLOD(const glm::vec3& cameraPosition);
    void performFrustumCulling(const glm::mat4& viewProjection);
    void updateSpatialPartitioning();

    // Procedural content generation
    void generateProceduralBuilding(const glm::vec3& position, BuildingType type);
    glm::vec3 generateTerrainHeight(float x, float z);
    void generateBuildingDetails(AdvancedBuilding& building);

public:
    CityManager(int seed = 42);
    ~CityManager();

    // Core generation methods
    void generateCity(int gridSize, float spacing);
    void spawnEntities(int numHumans, int numCars, int numTrees, int numFurniture);

    // Advanced features
    void generateComplexScene(int complexity = 3); // Generates 50+ objects
    void optimizeScene(const glm::vec3& cameraPosition, const glm::mat4& viewProjection);

    // Scene management
    void update(const glm::mat4& parentTransform = glm::mat4(1.0f)) override;
    void setLODDistances(float nearDist, float farDist);
    void enableSpatialPartitioning(bool enable);

    // Getters for rendering
    const std::vector<Road>& getRoads() const { return roads; }
    const std::vector<AdvancedBuilding>& getBuildings() const { return buildings; }
    const std::vector<std::unique_ptr<Entity>>& getEntities() const { return entities; }
    const std::vector<Entity*>& getVisibleEntities() const { return visibleEntities; }
    const std::vector<AdvancedBuilding*>& getVisibleBuildings() const { return visibleBuildings; }

    // Scene hierarchy access
    Node* getRootNode() const { return rootNode.get(); }
    Node* getBuildingsNode() const { return buildingsNode.get(); }
    Node* getEntitiesNode() const { return entitiesNode.get(); }

    // Statistics for performance monitoring
    struct SceneStats {
        int totalObjects;
        int visibleObjects;
        int hierarchyLevels;
        float lodNearCount;
        float lodFarCount;
        float frameTime;
    };
    SceneStats getSceneStatistics() const;
};