#include "Pedestrians.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <cmath>
#include "Shader.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Define external vector
std::vector<Pedestrian> pedestrians;

void spawnPedestrians(int count) {
    pedestrians.clear();
    srand(98765); // fixed seed for reproducibility
    int spawned = 0;
    int attempts = 0;
    while (spawned < count && attempts < count * 10) {
        attempts++;
        float x = -80.0f + (rand() % 161);
        float z = -80.0f + (rand() % 161);
        // Avoid roads and mall area (match conditions in main.cpp)
        if ((x >= -10.0f && x <= 10.0f) || (z >= -10.0f && z <= 10.0f)) continue;
        if (fmod(fabs(x), 40.0f) < 1.0f || fmod(fabs(z), 40.0f) < 1.0f) continue;
        float dxm = x - 60.0f; float dzm = z - 20.0f;
        if (dxm*dxm + dzm*dzm < 40.0f*40.0f) continue;
        float angle = (rand() % 360) * (M_PI / 180.0f);
        glm::vec3 dir = glm::normalize(glm::vec3(cos(angle), 0.0f, sin(angle)));
        float speed = 0.7f + (rand() % 100) / 100.0f * 0.9f;
        float dur = 1.0f + (rand() % 100) / 100.0f * 4.0f;
        glm::vec3 col = glm::vec3(0.7f - (rand()%40)/100.0f, 0.6f - (rand()%30)/100.0f, 0.5f - (rand()%30)/100.0f);
        pedestrians.emplace_back(glm::vec3(x, 0.0f, z), dir, speed, dur, col);
        spawned++;
    }
}

void updatePedestrians(float deltaTime) {
    for (auto &p : pedestrians) {
        p.walkTimer += deltaTime;
        // Advance walk cycle phase based on speed
        p.walkCyclePhase += deltaTime * p.speed * 3.0f; // 3.0f: controls walk speed
        if (p.walkCyclePhase > 2.0f * M_PI) p.walkCyclePhase -= 2.0f * M_PI;
        if (p.walkTimer > p.walkDuration) {
            float angle = (rand() % 360) * (M_PI / 180.0f);
            p.direction = glm::normalize(glm::vec3(cos(angle),0.0f,sin(angle)));
            p.walkDuration = 1.0f + (rand() % 100) / 100.0f * 4.0f;
            p.walkTimer = 0.0f;
        }
        glm::vec3 next = p.position + p.direction * p.speed * deltaTime;
        if (next.x < -120.0f || next.x > 120.0f || next.z < -120.0f || next.z > 120.0f) {
            p.direction = -p.direction;
            continue;
        }
        // Avoid roads and mall area - same checks as main
        if (!((next.x >= -10.0f && next.x <= 10.0f) || (next.z >= -10.0f && next.z <= 10.0f) ||
              fmod(fabs(next.x), 40.0f) < 1.0f || fmod(fabs(next.z), 40.0f) < 1.0f ||
              ((next.x-60.0f)*(next.x-60.0f)+(next.z-20.0f)*(next.z-20.0f) < 40.0f*40.0f))) {
            p.position = next;
        } else {
            float angle = (rand() % 180 - 90) * (M_PI/180.0f);
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0));
            glm::vec4 d = rot * glm::vec4(p.direction, 0.0f);
            p.direction = glm::normalize(glm::vec3(d));
        }
    }
}

void renderPedestrian(const Pedestrian& p, Shader& shader, unsigned int cubeVAO, unsigned int /*cylinderVAO*/) {
    glm::mat4 model;
    float facing = atan2(p.direction.x, p.direction.z);
    float bob = 0.03f * sin(2.0f * p.walkCyclePhase);

    // Minecraft-like proportions
    float torsoHeight = p.bodyHeight * 0.55f;
    float torsoWidth = 0.28f;
    float torsoDepth = 0.14f;
    float headSize = 0.22f;
    float armLength = torsoHeight * 0.95f;
    float armWidth = 0.10f;
    float armDepth = 0.10f;
    float legLength = p.bodyHeight * 0.45f;
    float legWidth = 0.11f;
    float legDepth = 0.11f;
    float handSize = 0.08f;

    // Torso (cube)
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(p.position.x, legLength + torsoHeight/2.0f + bob, p.position.z));
    model = glm::rotate(model, facing, glm::vec3(0,1,0));
    model = glm::scale(model, glm::vec3(torsoWidth, torsoHeight, torsoDepth));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", p.color);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Head (cube)
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(p.position.x, legLength + torsoHeight + headSize/2.0f + 0.03f + bob, p.position.z));
    model = glm::rotate(model, facing, glm::vec3(0,1,0));
    model = glm::scale(model, glm::vec3(headSize, headSize, headSize));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", glm::vec3(1.0f, 0.85f, 0.75f));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Arms (cubes, animated, with hands)
    float armSwing = glm::radians(45.0f) * sin(p.walkCyclePhase); // More pronounced pendulum swing
    for (int s = -1; s <=1; s+=2) {
        // Shoulder position: at top of torso, below head
        float shoulderY = legLength + torsoHeight - armWidth/2.0f + bob;
        float shoulderX = p.position.x + s * (torsoWidth/2.0f + armWidth/2.0f);
        glm::mat4 armModel = glm::mat4(1.0f);
        armModel = glm::translate(armModel, glm::vec3(shoulderX, shoulderY, p.position.z));
        armModel = glm::rotate(armModel, facing, glm::vec3(0,1,0));
        // Pendulum swing: rotate at shoulder, then move arm down
        armModel = glm::rotate(armModel, s*armSwing, glm::vec3(1,0,0));
        armModel = glm::translate(armModel, glm::vec3(0, -armLength/2.0f, 0));
        armModel = glm::scale(armModel, glm::vec3(armWidth, armLength, armDepth));
        shader.SetMat4("model", armModel);
        shader.SetVec3("objectColor", p.color * 0.9f);
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Hand (cube at end of arm)
        glm::mat4 handModel = glm::mat4(1.0f);
        handModel = glm::translate(handModel, glm::vec3(shoulderX, shoulderY, p.position.z));
        handModel = glm::rotate(handModel, facing, glm::vec3(0,1,0));
        handModel = glm::rotate(handModel, s*armSwing, glm::vec3(1,0,0));
        handModel = glm::translate(handModel, glm::vec3(0, -armLength, 0));
        handModel = glm::scale(handModel, glm::vec3(handSize, handSize, handSize));
        shader.SetMat4("model", handModel);
        shader.SetVec3("objectColor", glm::vec3(1.0f, 0.85f, 0.75f));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    // Legs (cubes, animated)
    float legSwing = glm::radians(35.0f) * sin(p.walkCyclePhase + M_PI);
    for (int s = -1; s <=1; s+=2) {
        model = glm::mat4(1.0f);
        float hipY = legLength/2.0f + bob;
        float hipX = p.position.x + s * (torsoWidth/2.0f - legWidth/2.0f);
        model = glm::translate(model, glm::vec3(hipX, hipY, p.position.z));
        model = glm::rotate(model, facing, glm::vec3(0,1,0));
        model = glm::translate(model, glm::vec3(0, legLength/2.0f, 0));
        model = glm::rotate(model, s*legSwing, glm::vec3(1,0,0));
        model = glm::translate(model, glm::vec3(0, -legLength/2.0f, 0));
        model = glm::scale(model, glm::vec3(legWidth, legLength, legDepth));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", p.color * 0.7f);
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}
