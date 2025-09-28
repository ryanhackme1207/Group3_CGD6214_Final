#pragma once
#include <vector>
#include <glm/glm.hpp>

// Simple quadtree node that stores 3D points with an integer id (index into external arrays)
class QuadNode {
public:
    QuadNode(const glm::vec2& center, float halfSize);
    ~QuadNode();

    // Insert point with associated id
    void Insert(const glm::vec3& point, int id);
    void Clear();

    // Query for points within an AABB centered at 'center' with half-size 'halfSize'
    // Returns matching ids in outIds and also fills outPoints with their positions if requested
    void QueryRange(const glm::vec2& center, float halfSize, std::vector<int>& outIds, std::vector<glm::vec3>* outPoints = nullptr);

private:
    glm::vec2 center; // XY = X,Z in world space
    float halfSize;
    struct Entry { glm::vec3 point; int id; };
    std::vector<Entry> points;
    QuadNode* children[4];
    bool divided;
};
