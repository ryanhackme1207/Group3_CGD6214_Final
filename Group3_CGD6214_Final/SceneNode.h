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
    glm::mat4 GetWorldTransform() const;

    void SetMesh(std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> GetMesh() const;

    void Draw(Shader& shader, const glm::mat4& parentTransform = glm::mat4(1.0f));

    std::string name;

private:
    SceneNode* parent;
    std::vector<std::shared_ptr<SceneNode>> children;
    glm::mat4 localTransform;
    std::shared_ptr<Mesh> mesh;
};
