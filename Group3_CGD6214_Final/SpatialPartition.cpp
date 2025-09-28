#include "SpatialPartition.h"
#include <algorithm>

QuadNode::QuadNode(const glm::vec2& c, float hs) : center(c), halfSize(hs), divided(false) {
    for (int i = 0; i < 4; i++) children[i] = nullptr;
}
QuadNode::~QuadNode() { Clear(); }

void QuadNode::Clear() {
    points.clear();
    for (int i = 0; i < 4; i++) {
        if (children[i]) { delete children[i]; children[i] = nullptr; }
    }
    divided = false;
}

void QuadNode::Insert(const glm::vec3& point) {
    if (fabs(point.x - center.x) > halfSize || fabs(point.z - center.y) > halfSize) return; // outside
    if (points.size() < 8) {
        points.push_back(point);
        return;
    }
    if (!divided) {
        float hs = halfSize / 2.0f;
        children[0] = new QuadNode(center + glm::vec2(-hs, -hs), hs);
        children[1] = new QuadNode(center + glm::vec2(hs, -hs), hs);
        children[2] = new QuadNode(center + glm::vec2(-hs, hs), hs);
        children[3] = new QuadNode(center + glm::vec2(hs, hs), hs);
        divided = true;
    }
    for (int i = 0; i < 4; i++) children[i]->Insert(point);
}

void QuadNode::QueryRange(const glm::vec2& c, float hs, std::vector<glm::vec3>& out) {
    // AABB overlap check
    if (center.x + halfSize < c.x - hs || center.x - halfSize > c.x + hs ||
        center.y + halfSize < c.y - hs || center.y - halfSize > c.y + hs) return;
    for (auto& p : points) {
        if (p.x >= c.x - hs && p.x <= c.x + hs && p.z >= c.y - hs && p.z <= c.y + hs) out.push_back(p);
    }
    if (divided) for (int i = 0; i < 4; i++) children[i]->QueryRange(c, hs, out);
}