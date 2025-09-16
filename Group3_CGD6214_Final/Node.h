#ifndef NODE_H
#define NODE_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Node {
public:
    glm::mat4 localTransform;
    glm::mat4 worldTransform;
    std::vector<Node*> children;

    Node();
    virtual ~Node();

    void addChild(Node* child);
    virtual void update(const glm::mat4& parentTransform = glm::mat4(1.0f));
};

#endif
