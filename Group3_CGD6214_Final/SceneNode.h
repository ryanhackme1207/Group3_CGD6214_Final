#pragma once
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "Mesh.h"

class Shader;

class SceneNode {
public:
    SceneNode();
    explicit SceneNode(const std::string& name);
    ~SceneNode();

    void AddChild(std::shared_ptr<SceneNode> child);
    void RemoveChild(std::shared_ptr<SceneNode> child);
    SceneNode* GetParent() const;
    const std::vector<std::shared_ptr<SceneNode>>& GetChildren() const;

    void SetLocalTransform(const glm::mat4& transform);
    const glm::mat4& GetLocalTransform() const;
    glm::mat4 GetWorldTransform();

    void SetMesh(std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> GetMesh() const;
    void SetColor(const glm::vec3& c) { nodeColor = c; useColor = true; }
    bool HasColor() const { return useColor; }
    glm::vec3 GetColor() const { return nodeColor; }

    // Draw uses parentTransform (default identity). Internal caching avoids recompute if not dirty.
    void Draw(Shader& shader, const glm::mat4& parentTransform = glm::mat4(1.0f));

    std::string name;

private:
    // mark this node and descendants dirty
    void MarkDirty();

    SceneNode* parent;
    std::vector<std::shared_ptr<SceneNode>> children;
    glm::mat4 localTransform;

    // cached world transform + dirty flag
    glm::mat4 cachedWorldTransform;
    bool worldDirty;

    std::shared_ptr<Mesh> mesh;

    // new: per-node color (optional)
    glm::vec3 nodeColor{ 1.0f,1.0f,1.0f };
    bool useColor = false;
};
