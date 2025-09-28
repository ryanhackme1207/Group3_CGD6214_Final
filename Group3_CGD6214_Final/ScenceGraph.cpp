#include "SceneGraph.h"

SceneGraph::SceneGraph() : root(std::make_shared<SceneNode>("Root")) {}
SceneGraph::~SceneGraph() {}

std::shared_ptr<SceneNode> SceneGraph::GetRoot() { return root; }

void SceneGraph::Draw(Shader& shader) {
    if (root) root->Draw(shader);
}