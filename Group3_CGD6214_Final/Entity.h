#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "Node.h"

enum class EntityType {
    TREE = 0,
    CAR = 1,
    HUMAN = 2,
    STREET_LIGHT = 3,
    BENCH = 4,
    LAMP_POST = 5,
    TRASH_BIN = 6,
    GRASS_PATCH = 7
};

enum class HumanSkinType {
    WHITE = 0,
    BLACK = 1,
    YELLOW = 2
};

struct HumanBodyPart {
    glm::vec3 localPosition;
    glm::vec3 scale;
    glm::vec3 rotation;
    glm::mat4 transform;

    HumanBodyPart(glm::vec3 pos, glm::vec3 sc)
        : localPosition(pos), scale(sc), rotation(0.0f), transform(1.0f) {
    }
};

class Entity : public Node {
public:
    EntityType type;
    glm::vec3 color;
    glm::vec3 velocity;
    glm::vec3 targetPosition;
    float speed;
    float animationTime;
    bool isMoving;
    bool visible;

    // Human-specific
    HumanSkinType skinType;
    std::vector<HumanBodyPart> bodyParts;
    float walkCycle;
    float stepHeight;
    bool isWalking;

    Entity();
    Entity(const glm::vec3& position, const glm::vec3& scale, EntityType t);
    ~Entity();

    void update(const glm::mat4& parentTransform) override;
    void setTarget(const glm::vec3& target);
    void moveRandomly();

    // Human rendering
    void renderHuman(unsigned int shaderProgram, unsigned int VAO,
        unsigned int whiteTexture, unsigned int blackTexture, unsigned int yellowTexture) const;
    void renderBodyPart(const HumanBodyPart& part, unsigned int shaderProgram,
        unsigned int VAO, unsigned int texture) const;

private:
    void initializeHuman(HumanSkinType skin);
    glm::vec3 getSkinColor(HumanSkinType skin);
    void updateMovement();
    void updateHumanAnimation();
    void updateBodyPartTransforms();
};