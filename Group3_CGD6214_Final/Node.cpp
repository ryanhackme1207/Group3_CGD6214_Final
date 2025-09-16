#include "Node.h"

Node::Node() : localTransform(1.0f), worldTransform(1.0f) {}

Node::~Node() {
    // IMPORTANT: we do NOT delete children here because ownership
    // is mixed (some children allocated on stack, some with new).
    // Manage memory externally to avoid double-free/crash.
}

void Node::addChild(Node* child) {
    if (child) children.push_back(child);
}

void Node::update(const glm::mat4& parentTransform) {
    worldTransform = parentTransform * localTransform;
    for (Node* child : children) {
        if (child) child->update(worldTransform);
    }
}
