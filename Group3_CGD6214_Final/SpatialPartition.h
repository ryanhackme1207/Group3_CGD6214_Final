#pragma once
#include <vector>
#include <glm/glm.hpp>

class QuadNode {
public:
    QuadNode(const glm::vec2& center, float halfSize);
    ~QuadNode();

    void Insert(const glm::vec3& point);
    void Clear();

    void QueryRange(const glm::vec2& center, float halfSize, std::vector<glm::vec3>& out);

private:
    glm::vec2 center;
    float halfSize;
    std::vector<glm::vec3> points;
    QuadNode* children[4];
    bool divided;
};
