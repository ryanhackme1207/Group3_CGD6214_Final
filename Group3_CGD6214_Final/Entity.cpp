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
    targetPosition = glm::vec3(0.0f);
    localTransform = glm::mat4(1.0f);
    worldTransform = glm::mat4(1.0f);

    // Human-specific initialization - properly initialize skinType
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
    targetPosition = position;
    walkCycle = 0.0f;
    stepHeight = 0.0f;
    isWalking = false;
    skinType = HumanSkinType::WHITE; // Initialize skinType

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
        speed = 1.5f + ((rand() % 20) / 100.0f); // Normal walking speed: 1.5-1.7
    }
    else if (type == EntityType::CAR) {
        color = glm::vec3(
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f
        );
        speed = 4.0f + ((rand() % 30) / 100.0f); // Normal car speed: 4.0-4.3
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

    bodyParts.push_back(HumanBodyPart(glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.4f, 0.4f, 0.4f))); // Head
    bodyParts.push_back(HumanBodyPart(glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0.4f, 0.6f, 0.2f))); // Torso
    bodyParts.push_back(HumanBodyPart(glm::vec3(-0.3f, 0.9f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f))); // Left arm
    bodyParts.push_back(HumanBodyPart(glm::vec3(0.3f, 0.9f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f)));  // Right arm
    bodyParts.push_back(HumanBodyPart(glm::vec3(-0.1f, 0.25f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f))); // Left leg
    bodyParts.push_back(HumanBodyPart(glm::vec3(0.1f, 0.25f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f)));   // Right leg
}

glm::vec3 Entity::getSkinColor(HumanSkinType skin) {
    if (skin == HumanSkinType::WHITE) return glm::vec3(0.95f, 0.87f, 0.8f);
    if (skin == HumanSkinType::BLACK) return glm::vec3(0.4f, 0.28f, 0.2f);
    if (skin == HumanSkinType::YELLOW) return glm::vec3(0.98f, 0.85f, 0.65f);
    return glm::vec3(0.95f, 0.87f, 0.8f);
}

bool Entity::isWithinCityBounds(const glm::vec3& position) {
    const float cityLimit = 44.0f; // More conservative limit
    return (fabsf(position.x) <= cityLimit && fabsf(position.z) <= cityLimit);
}

glm::vec3 Entity::constrainToCityBounds(const glm::vec3& position) {
    const float cityLimit = 44.0f;
    glm::vec3 constrainedPos = position;

    if (fabsf(constrainedPos.x) > cityLimit) {
        constrainedPos.x = (constrainedPos.x > 0) ? cityLimit : -cityLimit;
    }
    if (fabsf(constrainedPos.z) > cityLimit) {
        constrainedPos.z = (constrainedPos.z > 0) ? cityLimit : -cityLimit;
    }

    return constrainedPos;
}

void Entity::update(const glm::mat4& parentTransform) {
    animationTime += 0.016f;

    if (type == EntityType::HUMAN || type == EntityType::CAR) {
        updateMovement();

        if (type == EntityType::HUMAN) {
            updateHumanAnimation();
            updateBodyPartTransforms();
        }

        // Check if entity needs new target - less frequent for smoother behavior
        if (fmod(animationTime, 4.0f + (rand() % 200) / 100.0f) < 0.016f) {
            if (!isMoving || glm::length(velocity) < 0.1f) {
                moveRandomly();
            }
        }
    }

    Node::update(parentTransform);
}

void Entity::updateHumanAnimation() {
    if (isMoving && glm::length(velocity) > 0.1f) {
        walkCycle += 6.0f * 0.016f; // Smooth walking animation
        isWalking = true;
        stepHeight = 0.0f;

        float armSwing = sinf(walkCycle) * 0.3f; // Natural arm swing
        float legSwing = sinf(walkCycle) * 0.25f; // Natural leg swing

        bodyParts[2].rotation.x = armSwing;
        bodyParts[3].rotation.x = -armSwing;
        bodyParts[4].rotation.x = legSwing;
        bodyParts[5].rotation.x = -legSwing;

        bodyParts[1].localPosition.y = 0.8f + sinf(walkCycle * 2.0f) * 0.02f; // Natural bounce
        bodyParts[0].localPosition.y = 1.5f + sinf(walkCycle * 2.0f) * 0.02f;
    }
    else {
        isWalking = false;
        walkCycle = 0.0f;
        stepHeight = 0.0f;

        // Reset to neutral pose
        for (auto& part : bodyParts) part.rotation = glm::vec3(0.0f);

        bodyParts[0].localPosition.y = 1.5f;
        bodyParts[1].localPosition.y = 0.8f;
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
    // Ensure target is within city bounds
    glm::vec3 constrainedTarget = constrainToCityBounds(target);
    targetPosition = constrainedTarget;
    isMoving = true;

    glm::vec3 currentPos = glm::vec3(localTransform[3]);
    glm::vec3 direction = targetPosition - currentPos;
    if (glm::length(direction) > 0.1f) {
        velocity = glm::normalize(direction) * speed;
    }
}

void Entity::moveRandomly() {
    if (type == EntityType::TREE || type == EntityType::GRASS_PATCH ||
        type == EntityType::LAMP_POST || type == EntityType::TRASH_BIN) return;

    // Appropriate movement range based on entity type
    float range = (type == EntityType::CAR) ? 30.0f : 15.0f;
    glm::vec3 currentPos = glm::vec3(localTransform[3]);

    // Generate random target with more variety
    glm::vec3 randomOffset(
        ((rand() % 200) - 100) / 100.0f * range,  // -range to +range
        0.0f,
        ((rand() % 200) - 100) / 100.0f * range   // -range to +range
    );

    glm::vec3 randomTarget = currentPos + randomOffset;

    // Set appropriate Y position
    randomTarget.y = (type == EntityType::HUMAN) ? -0.9f : -0.8f;

    // Ensure the target is within bounds
    randomTarget = constrainToCityBounds(randomTarget);

    setTarget(randomTarget);
}

void Entity::updateMovement() {
    if (!isMoving) return;

    glm::vec3 currentPos = glm::vec3(localTransform[3]);
    glm::vec3 direction = targetPosition - currentPos;
    float distance = glm::length(direction);

    if (distance < 1.0f) { // Reached target
        isMoving = false;
        velocity = glm::vec3(0.0f);
        return;
    }

    // Smooth movement with proper speed scaling
    glm::vec3 movement = glm::normalize(direction) * speed * 0.016f;
    glm::vec3 newPos = currentPos + movement;

    // Check if the new position would be within city bounds
    if (!isWithinCityBounds(newPos)) {
        // Stop movement and find a new target within bounds
        isMoving = false;
        velocity = glm::vec3(0.0f);

        // Force entity to stay within bounds
        newPos = constrainToCityBounds(currentPos);

        // Set a new random target that's definitely within bounds
        moveRandomly();
        return;
    }

    // Update transform based on entity type
    localTransform = glm::mat4(1.0f);

    if (type == EntityType::HUMAN) {
        newPos.y = -0.9f + stepHeight; // Ensure humans stay at ground level

        // Handle rotation for humans - smooth turning
        if (glm::length(movement) > 0.01f && isMoving) {
            float angle = atan2f(movement.x, movement.z);
            localTransform = glm::rotate(localTransform, angle, glm::vec3(0, 1, 0));
        }

        localTransform = glm::translate(localTransform, newPos);
        glm::vec3 scale = glm::vec3(0.4f, 1.8f, 0.4f);
        localTransform = glm::scale(localTransform, scale);
    }
    else if (type == EntityType::CAR) {
        newPos.y = -0.8f; // Ensure cars stay at road level

        // Handle rotation for cars - smooth turning
        if (glm::length(movement) > 0.01f && isMoving) {
            float angle = atan2f(movement.x, movement.z);
            localTransform = glm::rotate(localTransform, angle, glm::vec3(0, 1, 0));
        }

        localTransform = glm::translate(localTransform, newPos);
        glm::vec3 scale = glm::vec3(2.0f, 1.0f, 4.0f);
        localTransform = glm::scale(localTransform, scale);
    }
    else {
        localTransform = glm::translate(localTransform, newPos);
    }
}

void Entity::renderHuman(unsigned int shaderProgram, unsigned int VAO,
    unsigned int whiteTexture, unsigned int blackTexture, unsigned int yellowTexture) {
    if (type != EntityType::HUMAN) return;

    unsigned int skinTexture;
    if (skinType == HumanSkinType::WHITE) skinTexture = whiteTexture;
    else if (skinType == HumanSkinType::BLACK) skinTexture = blackTexture;
    else if (skinType == HumanSkinType::YELLOW) skinTexture = yellowTexture;
    else skinTexture = whiteTexture;

    for (const auto& part : bodyParts) {
        renderBodyPart(part, shaderProgram, VAO, skinTexture);
    }
}

void Entity::renderBodyPart(const HumanBodyPart& part, unsigned int shaderProgram,
    unsigned int VAO, unsigned int texture) {
    glm::mat4 partModel = worldTransform * part.transform;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(partModel));
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(color));
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}