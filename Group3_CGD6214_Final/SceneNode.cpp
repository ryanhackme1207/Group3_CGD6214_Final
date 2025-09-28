#include "SceneNode.h"
#include "Shader.h"
#include <algorithm>

SceneNode::SceneNode() : parent(nullptr), localTransform(glm::mat4(1.0f)), cachedWorldTransform(glm::mat4(1.0f)), worldDirty(true) {}
SceneNode::SceneNode(const std::string& name) : parent(nullptr), name(name), localTransform(glm::mat4(1.0f)), cachedWorldTransform(glm::mat4(1.0f)), worldDirty(true) {}
SceneNode::~SceneNode() {}

void SceneNode::AddChild(std::shared_ptr<SceneNode> child) {
    if (child) {
        child->parent = this;
        children.push_back(child);
        child->MarkDirty();
    }
}

void SceneNode::RemoveChild(std::shared_ptr<SceneNode> child) {
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
    if (child) {
        child->parent = nullptr;
        child->MarkDirty();
    }
}

SceneNode* SceneNode::GetParent() const { return parent; }
const std::vector<std::shared_ptr<SceneNode>>& SceneNode::GetChildren() const { return children; }

void SceneNode::SetLocalTransform(const glm::mat4& transform) { localTransform = transform; MarkDirty(); }
const glm::mat4& SceneNode::GetLocalTransform() const { return localTransform; }

// Compute or return cached world transform. Non-const so cache can be updated.
glm::mat4 SceneNode::GetWorldTransform() {
    if (worldDirty) {
        if (parent) cachedWorldTransform = parent->GetWorldTransform() * localTransform;
        else cachedWorldTransform = localTransform;
        worldDirty = false;
    }
    return cachedWorldTransform;
}

void SceneNode::MarkDirty() {
    if (!worldDirty) {
        worldDirty = true;
        for (auto &c : children) if (c) c->MarkDirty();
    }
}

void SceneNode::SetMesh(std::shared_ptr<Mesh> mesh_) { mesh = mesh_; }
std::shared_ptr<Mesh> SceneNode::GetMesh() const { return mesh; }

void SceneNode::Draw(Shader& shader, const glm::mat4& parentTransform) {
    // compute world transform using cached value to avoid repeated multiplies
    glm::mat4 world;
    if (parent != nullptr) {
        // if parent provided, use that as base to avoid recalculating parent's transform
        world = parentTransform * localTransform;
    } else {
        world = GetWorldTransform();
    }

    if (mesh) mesh->Draw(shader, world);
    for (auto& child : children) child->Draw(shader, world);
}
