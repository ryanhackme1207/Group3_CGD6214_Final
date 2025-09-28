#include "SpatialPartition.h"
#include <algorithm>
#include <cmath>

static const size_t NODE_CAPACITY = 8;

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

void QuadNode::Insert(const glm::vec3& point, int id) {
    // point.x => world X, point.z => world Z stored in center as (x,z)
    if (std::abs(point.x - center.x) > halfSize || std::abs(point.z - center.y) > halfSize) return; // outside
    if (points.size() < NODE_CAPACITY) {
        points.push_back({point, id});
        return;
    }
    if (!divided) {
        float hs = halfSize / 2.0f;
        children[0] = new QuadNode(center + glm::vec2(-hs, -hs), hs);
        children[1] = new QuadNode(center + glm::vec2(hs, -hs), hs);
        children[2] = new QuadNode(center + glm::vec2(-hs, hs), hs);
        children[3] = new QuadNode(center + glm::vec2(hs, hs), hs);
        divided = true;
        // push existing points into children
        for (const auto &e : points) {
            for (int i = 0; i < 4; ++i) children[i]->Insert(e.point, e.id);
        }
        points.clear();
    }
    for (int i = 0; i < 4; i++) children[i]->Insert(point, id);
}

void QuadNode::QueryRange(const glm::vec2& c, float hs, std::vector<int>& outIds, std::vector<glm::vec3>* outPoints) {
    // AABB overlap check (this node vs query)
    if (center.x + halfSize < c.x - hs || center.x - halfSize > c.x + hs ||
        center.y + halfSize < c.y - hs || center.y - halfSize > c.y + hs) return;
    for (const auto& e : points) {
        if (e.point.x >= c.x - hs && e.point.x <= c.x + hs && e.point.z >= c.y - hs && e.point.z <= c.y + hs) {
            outIds.push_back(e.id);
            if (outPoints) outPoints->push_back(e.point);
        }
    }
    if (divided) for (int i = 0; i < 4; i++) children[i]->QueryRange(c, hs, outIds, outPoints);
}