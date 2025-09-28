#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <GL/glew.h>

class Shader;

struct Car {
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 color;
    float speed;
    float width;
    float height;
    float length;
    int lane;
    int roadType;
    int carType;

    Car() : position(0.0f), direction(0.0f), color(1.0f), speed(0.0f), width(0.0f), height(0.0f), length(0.0f), lane(0), roadType(0), carType(0) {}
    Car(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& col, float spd, int ln, int rt, int ct)
        : position(pos), direction(dir), color(col), speed(spd), width(0.0f), height(0.0f), length(0.0f), lane(ln), roadType(rt), carType(ct) {}
};

extern std::vector<Car> cars;
extern std::vector<Car> parkedCars;
extern float carSpawnTimer;
extern float timeOfDay; // day-night value from main

// Functions implemented in Traffic.cpp
void updateCars(float deltaTime);
void spawnCar();
void renderRoadInfrastructure(Shader& shader, GLuint cubeVAO, float currentTime);
void renderTrees(Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);
