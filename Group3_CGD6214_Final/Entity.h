#pragma once

#include "Node.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

enum class EntityType {
    HUMAN,
    CAR,
    TREE,
    GRASS_PATCH,
    LAMP_POST,
    TRASH_BIN
};

enum class HumanSkinType {
    WHITE,
    BLACK,
    YELLOW
};

struct HumanBodyPart {
    glm::vec3 localPosition;
    glm::vec3 scale;
    glm::vec3 rotation;
    glm::mat4 transform;

    HumanBodyPart(const glm::vec3& pos, const glm::vec3& s)
        : localPosition(pos), scale(s), rotation(0.0f), transform(1.0f) {
    }
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

    // Human-specific properties
    HumanSkinType skinType;
    std::vector<HumanBodyPart> bodyParts;
    float walkCycle;
    float stepHeight;
    bool isWalking;

    // Constructors and destructor
    Entity();
    Entity(const glm::vec3& position, const glm::vec3& scale, EntityType t);
    ~Entity();

    // Core methods
    void update(const glm::mat4& parentTransform) override;
    void setTarget(const glm::vec3& target);
    void moveRandomly();

    // Human-specific methods
    void initializeHuman(HumanSkinType skin);
    glm::vec3 getSkinColor(HumanSkinType skin);
    void updateHumanAnimation();
    void updateBodyPartTransforms();
    void renderHuman(unsigned int shaderProgram, unsigned int VAO,
        unsigned int whiteTexture, unsigned int blackTexture, unsigned int yellowTexture);
    void renderBodyPart(const HumanBodyPart& part, unsigned int shaderProgram,
        unsigned int VAO, unsigned int texture);

    // Boundary constraint methods (NEW)
    bool isWithinCityBounds(const glm::vec3& position);
    glm::vec3 constrainToCityBounds(const glm::vec3& position);

private:
    // Movement methods
    void updateMovement();
};