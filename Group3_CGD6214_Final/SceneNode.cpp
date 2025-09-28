#include "SceneNode.h"
#include "Shader.h"
#include <algorithm>

SceneNode::SceneNode() : parent(nullptr), localTransform(glm::mat4(1.0f)) {}
SceneNode::SceneNode(const std::string& name) : parent(nullptr), name(name), localTransform(glm::mat4(1.0f)) {}
SceneNode::~SceneNode() {}

void SceneNode::AddChild(std::shared_ptr<SceneNode> child) {
    if (child) {
        child->parent = this;
        children.push_back(child);
    }
}

void SceneNode::RemoveChild(std::shared_ptr<SceneNode> child) {
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
    if (child) child->parent = nullptr;
}

SceneNode* SceneNode::GetParent() const { return parent; }
const std::vector<std::shared_ptr<SceneNode>>& SceneNode::GetChildren() const { return children; }

void SceneNode::SetLocalTransform(const glm::mat4& transform) { localTransform = transform; }
const glm::mat4& SceneNode::GetLocalTransform() const { return localTransform; }

glm::mat4 SceneNode::GetWorldTransform() const {
    if (parent) return parent->GetWorldTransform() * localTransform;
    return localTransform;
}

void SceneNode::SetMesh(std::shared_ptr<Mesh> mesh_) { mesh = mesh_; }
std::shared_ptr<Mesh> SceneNode::GetMesh() const { return mesh; }

void SceneNode::Draw(Shader& shader, const glm::mat4& parentTransform) {
    glm::mat4 world = parentTransform * localTransform;
    if (mesh) mesh->Draw(shader, world);
    for (auto& child : children) child->Draw(shader, world);
}
