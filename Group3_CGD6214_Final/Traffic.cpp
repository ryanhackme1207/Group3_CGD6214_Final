#include "Traffic.h"
#include "Shader.h"
#include <glm/glm.hpp>
#include <cstdlib>
#include <cmath>

// Use the extern variables declared in Traffic.h which are defined in main.cpp

void updateCars(float deltaTime) {
    for (auto it = cars.begin(); it != cars.end();) {
        Car& car = *it;
        car.position += car.direction * car.speed * deltaTime;
        if (std::abs(car.position.x) > 150.0f || std::abs(car.position.z) > 150.0f) {
            it = cars.erase(it);
        } else {
            ++it;
        }
    }
}

void spawnCar() {
    if (cars.size() >= 40) return;
    glm::vec3 carColors[] = {
        glm::vec3(0.1f, 0.1f, 0.1f),
        glm::vec3(0.9f, 0.9f, 0.9f),
        glm::vec3(0.7f, 0.7f, 0.7f),
        glm::vec3(0.8f, 0.1f, 0.1f),
        glm::vec3(0.1f, 0.3f, 0.8f),
        glm::vec3(0.2f, 0.2f, 0.2f),
        glm::vec3(0.6f, 0.3f, 0.1f),
        glm::vec3(0.1f, 0.5f, 0.2f),
        glm::vec3(0.8f, 0.8f, 0.1f),
        glm::vec3(0.5f, 0.1f, 0.5f)
    };

    int colorIndex = rand() % 10;
    int roadType = rand() % 2;
    int lane = rand() % 2;
    int carType = rand() % 4;
    float speed = 18.0f + (rand() % 8) * 2.5f;

    glm::vec3 position, direction;
    switch (roadType) {
    case 0:
        if (lane == 0) { position = glm::vec3(-120.0f, 0.8f, -6.0f); direction = glm::vec3(1.0f,0.0f,0.0f); }
        else { position = glm::vec3(120.0f,0.8f,6.0f); direction = glm::vec3(-1.0f,0.0f,0.0f); }
        break;
    case 1:
        if (lane == 0) { position = glm::vec3(6.0f,0.8f,-120.0f); direction = glm::vec3(0.0f,0.0f,1.0f); }
        else { position = glm::vec3(-6.0f,0.8f,120.0f); direction = glm::vec3(0.0f,0.0f,-1.0f); }
        break;
    }
    cars.emplace_back(position, direction, carColors[colorIndex], speed, lane, roadType, carType);
}

// Helper: render a box at position with color
static void DrawBox(Shader& shader, const glm::vec3& pos, const glm::vec3& scale, const glm::vec3& color, GLuint cubeVAO) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::scale(model, scale);
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", color);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void renderRoadInfrastructure(Shader& shader, GLuint cubeVAO, float currentTime) {
    // Animate street light brightness based on timeOfDay (extern)
    float skyIntensity = (timeOfDay >= 6.0f && timeOfDay <= 18.0f) ? 0.6f : 0.1f;
    float lightScale = glm::clamp(1.0f - (skyIntensity - 0.1f) / 0.7f, 0.0f, 1.0f);

    // --- STREET LIGHTS ---
    for (int i = -120; i <= 120; i += 30) {
        for (int side = -1; side <= 1; side += 2) {
            float zPos = side * 15.0f;
            // pole
            DrawBox(shader, glm::vec3(i, 5.0f, zPos), glm::vec3(0.2f, 10.0f, 0.2f), glm::vec3(0.4f), cubeVAO);
            // lamp head
            float flicker = 0.85f + 0.15f * sin(currentTime * 3.0f + i * 0.1f);
            glm::vec3 lampColor = glm::vec3(1.0f, 0.95f, 0.8f) * (flicker * lightScale);
            DrawBox(shader, glm::vec3(i, 9.5f, zPos), glm::vec3(1.0f, 0.6f, 1.0f), lampColor, cubeVAO);

            // Add a glow quad (approx) - rendered as slightly larger translucent cube
            if (lightScale > 0.01f) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glm::vec3 glowColor = lampColor;
                shader.SetVec3("objectColor", glowColor * 0.6f);
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(i, 9.5f, zPos));
                model = glm::scale(model, glm::vec3(2.2f, 1.8f, 2.2f));
                shader.SetMat4("model", model);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glDisable(GL_BLEND);
            }
        }
    }

    // --- TRAFFIC SIGNALS ---
    std::vector<glm::vec3> positions = {
        glm::vec3(-20.0f, 0.0f, -20.0f),
        glm::vec3(20.0f, 0.0f, -20.0f),
        glm::vec3(-20.0f, 0.0f, 20.0f),
        glm::vec3(20.0f, 0.0f, 20.0f)
    };

    for (const auto& p : positions) {
        // pole
        DrawBox(shader, glm::vec3(p.x, 4.0f, p.z), glm::vec3(0.3f, 8.0f, 0.3f), glm::vec3(0.3f), cubeVAO);
        // housing
        DrawBox(shader, glm::vec3(p.x, 7.0f, p.z), glm::vec3(0.8f, 2.0f, 0.8f), glm::vec3(0.12f,0.12f,0.12f), cubeVAO);

        // Light cycle: 9-second cycle as in main
        float cycle = fmod(currentTime, 9.0f);
        glm::vec3 activeColor;
        if (cycle < 3.0f) activeColor = glm::vec3(0.9f,0.1f,0.1f); // red
        else if (cycle < 4.0f) activeColor = glm::vec3(0.9f,0.9f,0.1f); // yellow
        else activeColor = glm::vec3(0.1f,0.9f,0.1f); // green

        // draw active light as small glowing box
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        DrawBox(shader, glm::vec3(p.x, 7.0f, p.z+0.5f), glm::vec3(0.4f, 0.4f, 0.1f), activeColor, cubeVAO);
        glDisable(GL_BLEND);
    }
}

void renderTrees(Shader& shader, GLuint /*cubeVAO*/, GLuint /*cylinderVAO*/) {
    // Placeholder - trees rendered in main.cpp or elsewhere
}
