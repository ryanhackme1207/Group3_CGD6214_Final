#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"

struct Pedestrian {
    glm::vec3 position;
    glm::vec3 direction;
    float speed;
    float walkTimer;
    float walkDuration;
    float bodyHeight;
    glm::vec3 color;
    float walkCyclePhase; // phase for limb animation
    Pedestrian(glm::vec3 pos = glm::vec3(0.0f), glm::vec3 dir = glm::vec3(0.0f), float spd = 0.0f, float dur = 0.0f, glm::vec3 col = glm::vec3(1.0f))
        : position(pos), direction(dir), speed(spd), walkTimer(0.0f), walkDuration(dur), bodyHeight(1.75f), color(col), walkCyclePhase(0.0f) {}
};

// Global pedestrians vector is declared in main.cpp; functions operate on it via extern
extern std::vector<Pedestrian> pedestrians;

// Function prototypes
void spawnPedestrians(int count);
void updatePedestrians(float deltaTime);
void renderPedestrian(const Pedestrian& p, Shader& shader, unsigned int cubeVAO, unsigned int cylinderVAO);
