#include "Entity.h"
#include <random>
#include <cmath>
#include <memory>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

Entity::Entity() {
    type = EntityType::HUMAN;
    color = glm::vec3(1.0f);
    velocity = glm::vec3(0.0f);
    speed = 2.0f;
    animationTime = 0.0f;
    isMoving = false;
    visible = true;
    targetPosition = glm::vec3(0.0f);
    localTransform = glm::mat4(1.0f);
    worldTransform = glm::mat4(1.0f);

    skinType = HumanSkinType::WHITE;
    walkCycle = 0.0f;
    stepHeight = 0.0f;
    isWalking = false;
    initializeHuman(skinType);
}

Entity::Entity(const glm::vec3& position, const glm::vec3& scale, EntityType t) {
    type = t;
    velocity = glm::vec3(0.0f);
    animationTime = 0.0f;
    isMoving = false;
    visible = true;
    targetPosition = position;
    walkCycle = 0.0f;
    stepHeight = 0.0f;
    isWalking = false;
    skinType = HumanSkinType::WHITE;

    localTransform = glm::mat4(1.0f);
    localTransform = glm::translate(localTransform, position);
    localTransform = glm::scale(localTransform, scale);
    worldTransform = glm::mat4(1.0f);

    if (type == EntityType::HUMAN) {
        int skinChoice = rand() % 3;
        if (skinChoice == 0) skinType = HumanSkinType::WHITE;
        else if (skinChoice == 1) skinType = HumanSkinType::BLACK;
        else skinType = HumanSkinType::YELLOW;

        initializeHuman(skinType);
        color = getSkinColor(skinType);
        speed = 0.3f + ((rand() % 30) / 100.0f);
    }
    else if (type == EntityType::CAR) {
        color = glm::vec3(
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f
        );
        speed = 1.0f + ((rand() % 50) / 100.0f);
    }
    else if (type == EntityType::TREE) {
        color = glm::vec3(0.2f, 0.4f, 0.1f);
        speed = 0.0f;
    }
    else if (type == EntityType::GRASS_PATCH) {
        color = glm::vec3(0.3f, 0.8f, 0.2f);
        speed = 0.0f;
    }
    else if (type == EntityType::LAMP_POST) {
        color = glm::vec3(0.7f, 0.7f, 0.7f);
        speed = 0.0f;
    }
    else if (type == EntityType::TRASH_BIN) {
        color = glm::vec3(0.3f, 0.3f, 0.3f);
        speed = 0.0f;
    }
    else {
        color = glm::vec3(1.0f);
        speed = 0.0f;
    }
}

Entity::~Entity() {
    bodyParts.clear();
}

void Entity::initializeHuman(HumanSkinType skin) {
    bodyParts.clear();
    // Head
    bodyParts.push_back(HumanBodyPart(glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.4f, 0.4f, 0.4f)));
    // Body
    bodyParts.push_back(HumanBodyPart(glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0.4f, 0.6f, 0.2f)));
    // Left arm
    bodyParts.push_back(HumanBodyPart(glm::vec3(-0.3f, 0.9f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f)));
    // Right arm
    bodyParts.push_back(HumanBodyPart(glm::vec3(0.3f, 0.9f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f)));
    // Left leg
    bodyParts.push_back(HumanBodyPart(glm::vec3(-0.1f, 0.25f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f)));
    // Right leg
    bodyParts.push_back(HumanBodyPart(glm::vec3(0.1f, 0.25f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f)));
}

glm::vec3 Entity::getSkinColor(HumanSkinType skin) {
    switch (skin) {
    case HumanSkinType::WHITE: return glm::vec3(0.95f, 0.87f, 0.8f);
    case HumanSkinType::BLACK: return glm::vec3(0.4f, 0.28f, 0.2f);
    case HumanSkinType::YELLOW: return glm::vec3(0.98f, 0.85f, 0.65f);
    default: return glm::vec3(0.95f, 0.87f, 0.8f);
    }
}

void Entity::update(const glm::mat4& parentTransform) {
    animationTime += 0.016f;

    if (type == EntityType::HUMAN || type == EntityType::CAR) {
        updateMovement();

        if (type == EntityType::HUMAN) {
            updateHumanAnimation();
            updateBodyPartTransforms();
        }

        // Random movement trigger
        if (fmod(animationTime, 8.0f + (rand() % 400) / 100.0f) < 0.016f) {
            moveRandomly();
        }
    }

    Node::update(parentTransform);
}

void Entity::updateHumanAnimation() {
    if (isMoving) {
        walkCycle += 1.5f * 0.016f;
        isWalking = true;
        stepHeight = 0.0f;

        float armSwing = sinf(walkCycle) * 0.2f;
        float legSwing = sinf(walkCycle) * 0.15f;

        // Animate arms and legs
        if (bodyParts.size() >= 6) {
            bodyParts[2].rotation.x = armSwing;   // Left arm
            bodyParts[3].rotation.x = -armSwing;  // Right arm
            bodyParts[4].rotation.x = legSwing;   // Left leg
            bodyParts[5].rotation.x = -legSwing;  // Right leg

            // Body bob
            bodyParts[1].localPosition.y = 0.8f + sinf(walkCycle * 2.0f) * 0.01f;
            bodyParts[0].localPosition.y = 1.5f + sinf(walkCycle * 2.0f) * 0.01f;
        }
    }
    else {
        isWalking = false;
        walkCycle = 0.0f;
        stepHeight = 0.0f;

        // Reset animations
        for (auto& part : bodyParts) {
            part.rotation = glm::vec3(0.0f);
        }
        if (bodyParts.size() >= 2) {
            bodyParts[0].localPosition.y = 1.5f;
            bodyParts[1].localPosition.y = 0.8f;
        }
    }
}

void Entity::updateBodyPartTransforms() {
    for (auto& part : bodyParts) {
        part.transform = glm::mat4(1.0f);
        part.transform = glm::translate(part.transform, part.localPosition);
        part.transform = glm::rotate(part.transform, part.rotation.x, glm::vec3(1, 0, 0));
        part.transform = glm::rotate(part.transform, part.rotation.y, glm::vec3(0, 1, 0));
        part.transform = glm::rotate(part.transform, part.rotation.z, glm::vec3(0, 0, 1));
        part.transform = glm::scale(part.transform, part.scale);
    }
}

void Entity::setTarget(const glm::vec3& target) {
    targetPosition = target;
    isMoving = true;
    glm::vec3 currentPos = glm::vec3(localTransform[3]);
    glm::vec3 direction = targetPosition - currentPos;
    if (glm::length(direction) > 0.1f) {
        velocity = glm::normalize(direction) * speed;
    }
}

void Entity::moveRandomly() {
    // Don't move static objects
    if (type == EntityType::TREE || type == EntityType::GRASS_PATCH ||
        type == EntityType::LAMP_POST || type == EntityType::TRASH_BIN) {
        return;
    }

    float range = (type == EntityType::CAR) ? 30.0f : 10.0f;
    glm::vec3 currentPos = glm::vec3(localTransform[3]);

    glm::vec3 randomTarget(
        currentPos.x + (-range + ((rand() % 100) / 100.0f) * 2.0f * range),
        currentPos.y,
        currentPos.z + (-range + ((rand() % 100) / 100.0f) * 2.0f * range)
    );

    // Keep within bounds
    randomTarget.x = glm::clamp(randomTarget.x, -40.0f, 40.0f);
    randomTarget.z = glm::clamp(randomTarget.z, -40.0f, 40.0f);
    randomTarget.y = (type == EntityType::HUMAN) ? -0.9f : -0.8f;

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

    glm::vec3 movement = glm::normalize(direction) * speed * 0.016f;
    currentPos += movement;

    // Update transform based on entity type
    localTransform = glm::mat4(1.0f);

    if (type == EntityType::HUMAN) {
        currentPos.y = -0.9f + stepHeight;

        // Rotate to face movement direction
        if (glm::length(glm::vec2(movement.x, movement.z)) > 0.01f) {
            float angle = atan2f(movement.x, movement.z);
            localTransform = glm::rotate(localTransform, angle, glm::vec3(0, 1, 0));
        }

        localTransform = glm::translate(localTransform, currentPos);
        localTransform = glm::scale(localTransform, glm::vec3(0.4f, 1.8f, 0.3f));
    }
    else if (type == EntityType::CAR) {
        currentPos.y = -0.8f;

        // Rotate to face movement direction
        if (glm::length(glm::vec2(movement.x, movement.z)) > 0.01f) {
            float angle = atan2f(movement.x, movement.z);
            localTransform = glm::rotate(localTransform, angle, glm::vec3(0, 1, 0));
        }

        localTransform = glm::translate(localTransform, currentPos);
        localTransform = glm::scale(localTransform, glm::vec3(1.5f, 0.8f, 3.0f));
    }
    else {
        localTransform = glm::translate(localTransform, currentPos);
    }
}

void Entity::renderHuman(unsigned int shaderProgram, unsigned int VAO,
    unsigned int whiteTexture, unsigned int blackTexture, unsigned int yellowTexture) const {

    if (type != EntityType::HUMAN) {
        // For non-humans, just render as simple object
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(worldTransform));
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(color));
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        return;
    }

    // Select appropriate skin texture
    unsigned int skinTexture;
    switch (skinType) {
    case HumanSkinType::WHITE: skinTexture = whiteTexture; break;
    case HumanSkinType::BLACK: skinTexture = blackTexture; break;
    case HumanSkinType::YELLOW: skinTexture = yellowTexture; break;
    default: skinTexture = whiteTexture; break;
    }

    // Render each body part
    for (const auto& part : bodyParts) {
        renderBodyPart(part, shaderProgram, VAO, skinTexture);
    }
}

void Entity::renderBodyPart(const HumanBodyPart& part, unsigned int shaderProgram,
    unsigned int VAO, unsigned int texture) const {

    glm::mat4 partModel = worldTransform * part.transform;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(partModel));
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(color));

    // Bind texture if provided
    if (texture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}