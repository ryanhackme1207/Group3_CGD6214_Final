#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Node.h"

enum class EntityType {
    HUMAN,
    CAR,
    TREE,
    GRASS_PATCH,
    LAMP_POST,
    TRASH_BIN
};

class Entity : public Node {
public:
    EntityType type;
    glm::vec3 color;
    glm::vec3 velocity;
    float speed;
    float animationTime;
    bool isMoving;
    glm::vec3 targetPosition;

    Entity();
    Entity(const glm::vec3& position, const glm::vec3& scale, EntityType t);
    virtual ~Entity();

    virtual void update(const glm::mat4& parentTransform = glm::mat4(1.0f)) override;
    void setTarget(const glm::vec3& target);
    void moveRandomly();

private:
    void updateMovement();
};