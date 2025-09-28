#pragma once
#include <memory>
#include "SceneNode.h"

class Shader;

class SceneGraph {
public:
    SceneGraph();
    ~SceneGraph();

    std::shared_ptr<SceneNode> GetRoot();
    void Draw(Shader& shader);

private:
    std::shared_ptr<SceneNode> root;
};
