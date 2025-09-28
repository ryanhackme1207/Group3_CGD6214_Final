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
    // Construct car then adjust physical dimensions for moving vehicles so they appear correctly sized
    Car c(position, direction, carColors[colorIndex], speed, lane, roadType, carType);
    // Increase size for moving vehicles so they're visible in the larger scene
    const float movingScale = 1.9f; // tweakable scale factor
    if (c.speed > 0.01f) {
        c.width *= movingScale;
        c.height *= movingScale;
        c.length *= movingScale;
    }
    cars.push_back(c);
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

// Implementation of renderShoppingMallComplex moved here to fix unresolved external
void renderShoppingMallComplex(Shader& shader, GLuint cubeVAO, GLuint cylinderVAO)
{
    glm::mat4 model;
    // Mall parameters
    glm::vec3 mallPos = glm::vec3(60.0f, 0.0f, 60.0f);
    float mallWidth = 60.0f;
    float mallDepth = 40.0f;
    float mallHeight = 15.0f;

    // Main mall building
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(mallPos.x, mallHeight / 2.0f, mallPos.z));
    model = glm::scale(model, glm::vec3(mallWidth, mallHeight, mallDepth));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", glm::vec3(0.85f, 0.85f, 0.9f));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Mall entrance canopy
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(mallPos.x, 2.0f, mallPos.z - mallDepth/2.0f - 3.0f));
    model = glm::scale(model, glm::vec3(20.0f, 1.0f, 6.0f));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", glm::vec3(0.2f, 0.2f, 0.25f));
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Row of small shops in front of mall
    int shopCount = 6;
    float shopWidth = mallWidth / (float)shopCount - 1.0f;
    float shopDepth = 8.0f;
    float shopHeight = 6.0f;
    float firstShopX = mallPos.x - mallWidth/2.0f + shopWidth/2.0f + 1.0f;
    for (int i = 0; i < shopCount; ++i) {
        float sx = firstShopX + i * (shopWidth + 1.0f);
        float sz = mallPos.z - mallDepth/2.0f - shopDepth/2.0f - 10.0f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(sx, shopHeight/2.0f, sz));
        model = glm::scale(model, glm::vec3(shopWidth, shopHeight, shopDepth));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.85f));
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Shop glass window (front)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(sx, shopHeight/2.0f - 0.5f, sz + shopDepth/2.0f + 0.01f));
        model = glm::scale(model, glm::vec3(shopWidth*0.8f, shopHeight*0.6f, 0.05f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.6f, 0.8f, 0.95f));
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Shop signage
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(sx, shopHeight - 0.6f, sz + shopDepth/2.0f + 0.02f));
        model = glm::scale(model, glm::vec3(shopWidth*0.6f, 0.6f, 0.02f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.1f, 0.4f, 0.8f));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Parking lot surface in front of shops
    float parkingWidth = mallWidth + 20.0f;
    float parkingDepth = 30.0f;
    glm::vec3 parkingCenter = glm::vec3(mallPos.x, 0.01f, mallPos.z - mallDepth/2.0f - 10.0f);
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(parkingCenter.x, 0.02f, parkingCenter.z));
    model = glm::scale(model, glm::vec3(parkingWidth, 0.04f, parkingDepth));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", glm::vec3(0.15f, 0.15f, 0.15f));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Parking lines
    int spacesPerRow = 10;
    float spaceWidth = (parkingWidth - 10.0f) / (float)spacesPerRow;
    float startLineX = parkingCenter.x - parkingWidth/2.0f + 5.0f + spaceWidth/2.0f;
    float lineZ = parkingCenter.z - parkingDepth/2.0f + 2.0f;
    for (int i = 0; i < spacesPerRow; ++i) {
        float lx = startLineX + i * spaceWidth;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(lx, 0.05f, lineZ));
        model = glm::scale(model, glm::vec3(spaceWidth*0.9f, 0.02f, 0.12f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.95f, 0.95f, 0.95f));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Render parked cars
    // forward declare renderRealisticCar from main.cpp; the symbol exists there
    extern void renderRealisticCar(const Car& car, Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);
    for (const auto& pcar : parkedCars) {
        renderRealisticCar(pcar, shader, cubeVAO, cylinderVAO);
    }
}
