#include "Entity.h"
#include <random>
#include <cmath>

Entity::Entity() {
    type = EntityType::HUMAN;
    color = glm::vec3(1.0f);
    velocity = glm::vec3(0.0f);
    speed = 2.0f;
    animationTime = 0.0f;
    isMoving = false;
    targetPosition = glm::vec3(0.0f);
    localTransform = glm::mat4(1.0f);
    worldTransform = glm::mat4(1.0f);
}

Entity::Entity(const glm::vec3& position, const glm::vec3& scale, EntityType t) {
    type = t;
    velocity = glm::vec3(0.0f);
    animationTime = 0.0f;
    isMoving = false;
    targetPosition = position;

    localTransform = glm::mat4(1.0f);
    localTransform = glm::translate(localTransform, position);
    localTransform = glm::scale(localTransform, scale);
    worldTransform = glm::mat4(1.0f);

    switch (type) {
    case EntityType::HUMAN:
        color = glm::vec3(0.8f, 0.6f, 0.4f); // Skin tone
        speed = 1.5f + ((rand() % 100) / 100.0f); // Varied walking speed
        break;
    case EntityType::CAR:
        color = glm::vec3(
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f
        ); // Random car colors
        speed = 8.0f + ((rand() % 300) / 100.0f);
        break;
    case EntityType::TREE:
        color = glm::vec3(0.2f, 0.4f, 0.1f); // Dark green
        speed = 0.0f; // Trees don't move
        break;
    case EntityType::GRASS_PATCH:
        color = glm::vec3(0.3f, 0.8f, 0.2f); // Bright green
        speed = 0.0f;
        break;
    case EntityType::LAMP_POST:
        color = glm::vec3(0.7f, 0.7f, 0.7f); // Gray metal
        speed = 0.0f;
        break;
    case EntityType::TRASH_BIN:
        color = glm::vec3(0.3f, 0.3f, 0.3f); // Dark gray
        speed = 0.0f;
        break;
    default:
        color = glm::vec3(1.0f);
        speed = 0.0f;
        break;
    }
}

Entity::~Entity() {
    // Nothing to free here
}

void Entity::update(const glm::mat4& parentTransform) {
    animationTime += 0.016f; // Assuming 60 FPS

    if (type == EntityType::HUMAN || type == EntityType::CAR) {
        updateMovement();

        // Random movement decision every 3-5 seconds
        if (fmod(animationTime, 3.0f + (rand() % 200) / 100.0f) < 0.016f) {
            moveRandomly();
        }
    }

    Node::update(parentTransform);
}

void Entity::setTarget(const glm::vec3& target) {
    targetPosition = target;
    isMoving = true;

    glm::vec3 direction = targetPosition - glm::vec3(localTransform[3]);
    if (glm::length(direction) > 0.1f) {
        velocity = glm::normalize(direction) * speed;
    }
}

void Entity::moveRandomly() {
    if (type == EntityType::TREE || type == EntityType::GRASS_PATCH ||
        type == EntityType::LAMP_POST || type == EntityType::TRASH_BIN) {
        return; // Static objects don't move
    }

    // Generate random target within reasonable bounds
    float range = (type == EntityType::CAR) ? 30.0f : 15.0f;
    glm::vec3 currentPos = glm::vec3(localTransform[3]);

    glm::vec3 randomTarget = glm::vec3(
        currentPos.x + ((rand() % 200 - 100) / 100.0f) * range,
        currentPos.y,
        currentPos.z + ((rand() % 200 - 100) / 100.0f) * range
    );

    // Keep entities within city bounds
    randomTarget.x = glm::clamp(randomTarget.x, -40.0f, 40.0f);
    randomTarget.z = glm::clamp(randomTarget.z, -40.0f, 40.0f);

    setTarget(randomTarget);
}

void Entity::updateMovement() {
    if (!isMoving) return;

    glm::vec3 currentPos = glm::vec3(localTransform[3]);
    glm::vec3 direction = targetPosition - currentPos;
    float distance = glm::length(direction);

    if (distance < 0.5f) {
        isMoving = false;
        velocity = glm::vec3(0.0f);
        return;
    }

    // Move towards target
    glm::vec3 movement = glm::normalize(direction) * speed * 0.016f;

    // Update local transform with new position
    localTransform = glm::mat4(1.0f);
    currentPos += movement;
    localTransform = glm::translate(localTransform, currentPos);

    // Scale based on entity type
    glm::vec3 scale;
    switch (type) {
    case EntityType::HUMAN:
        scale = glm::vec3(0.4f, 1.8f, 0.4f);
        // Add slight bobbing animation for walking
        localTransform = glm::translate(localTransform, glm::vec3(0, sin(animationTime * 8.0f) * 0.05f, 0));
        break;
    case EntityType::CAR:
        scale = glm::vec3(2.0f, 1.0f, 4.0f);
        // Rotate car to face movement direction
        if (glm::length(movement) > 0.01f) {
            float angle = atan2(movement.x, movement.z);
            localTransform = glm::rotate(localTransform, angle, glm::vec3(0, 1, 0));
        }
        break;
    default:
        scale = glm::vec3(1.0f);
        break;
    }

    localTransform = glm::scale(localTransform, scale);
}