#include "main.h"
#include "Shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

// Provide implementations for functions used by main.cpp that were causing linker errors

GLuint createGround()
{
    // Ground vertices (a large plane) - same as previous main.cpp's function
    static const float vertices[] = {
        // positions          // normals           // texture coords
        -1.0f, 0.0f, -1.0f,   0.0f,  1.0f, 0.0f,   0.0f, 0.0f,
         1.0f, 0.0f, -1.0f,   0.0f,  1.0f, 0.0f,   10.0f, 0.0f,
         1.0f, 0.0f,  1.0f,   0.0f,  1.0f, 0.0f,   10.0f, 1.0f,
        -1.0f, 0.0f,  1.0f,   0.0f,  1.0f, 0.0f,   0.0f, 1.0f,
    };

    unsigned int indices[] = { 0,1,2, 0,2,3 };
    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    return VAO;
}

GLuint createTriangularRoof()
{
    static const float vertices[] = { /* simplified roof verts */
        -0.5f, -0.5f,  0.5f,  0,0,1, 0,0,
         0.5f, -0.5f,  0.5f,  0,0,1, 1,0,
         0.0f,  0.5f,  0.0f,  0,0,1, 0.5f,1,

         0.5f, -0.5f, -0.5f,  0,0,-1, 0,0,
        -0.5f, -0.5f, -0.5f,  0,0,-1, 1,0,
         0.0f,  0.5f,  0.0f,  0,0,-1, 0.5f,1,

        -0.5f, -0.5f, -0.5f, -0.707f,0.707f,0, 0,0,
        -0.5f, -0.5f,  0.5f, -0.707f,0.707f,0, 1,0,
         0.0f,  0.5f,  0.0f, -0.707f,0.707f,0, 0.5f,1,

         0.5f, -0.5f,  0.5f, 0.707f,0.707f,0, 0,0,
         0.5f, -0.5f, -0.5f, 0.707f,0.707f,0, 1,0,
         0.0f,  0.5f,  0.0f, 0.707f,0.707f,0, 0.5f,1,

        -0.5f, -0.5f, -0.5f,  0,-1,0, 0,0,
         0.5f, -0.5f, -0.5f,  0,-1,0, 1,0,
         0.5f, -0.5f,  0.5f,  0,-1,0, 1,1,

         0.5f, -0.5f,  0.5f,  0,-1,0, 1,1,
        -0.5f, -0.5f,  0.5f,  0,-1,0, 0,1,
        -0.5f, -0.5f, -0.5f,  0,-1,0, 0,0
    };
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    return VAO;
}

GLuint createCylinder()
{
    const int segments = 16;
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    float h = 1.0f; float r = 0.5f;
    // side vertices
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * 3.14159265f;
        float x = cos(theta) * r;
        float z = sin(theta) * r;
        // bottom
        verts.push_back(x); verts.push_back(-h / 2.0f); verts.push_back(z);
        verts.push_back(x); verts.push_back(0.0f); verts.push_back(z); // normal
        verts.push_back((float)i / segments); verts.push_back(0.0f);
        // top
        verts.push_back(x); verts.push_back(h / 2.0f); verts.push_back(z);
        verts.push_back(x); verts.push_back(0.0f); verts.push_back(z);
        verts.push_back((float)i / segments); verts.push_back(1.0f);
    }
    // indices for sides
    for (int i = 0; i < segments; ++i) {
        unsigned int a = i * 2;
        unsigned int b = a + 1;
        unsigned int c = (i + 1) * 2;
        unsigned int d = c + 1;
        inds.push_back(a); inds.push_back(c); inds.push_back(b);
        inds.push_back(b); inds.push_back(c); inds.push_back(d);
    }
    // caps (center + rim)
    unsigned int baseIndex = (unsigned int)verts.size() / 8;
    // center bottom
    verts.push_back(0.0f); verts.push_back(-h / 2.0f); verts.push_back(0.0f);
    verts.push_back(0.0f); verts.push_back(-1.0f); verts.push_back(0.0f);
    verts.push_back(0.5f); verts.push_back(0.5f);
    // rim bottom
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * 3.14159265f;
        float x = cos(theta) * r;
        float z = sin(theta) * r;
        verts.push_back(x); verts.push_back(-h / 2.0f); verts.push_back(z);
        verts.push_back(0.0f); verts.push_back(-1.0f); verts.push_back(0.0f);
        verts.push_back((x / r + 1.0f) * 0.5f); verts.push_back((z / r + 1.0f) * 0.5f);
    }
    unsigned int centerBottom = baseIndex;
    for (int i = 0; i < segments; ++i) {
        inds.push_back(centerBottom);
        inds.push_back(centerBottom + 1 + i);
        inds.push_back(centerBottom + 1 + i + 1);
    }
    // top cap
    baseIndex = (unsigned int)verts.size() / 8;
    // center top
    verts.push_back(0.0f); verts.push_back(h / 2.0f); verts.push_back(0.0f);
    verts.push_back(0.0f); verts.push_back(1.0f); verts.push_back(0.0f);
    verts.push_back(0.5f); verts.push_back(0.5f);
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * 3.14159265f;
        float x = cos(theta) * r;
        float z = sin(theta) * r;
        verts.push_back(x); verts.push_back(h / 2.0f); verts.push_back(z);
        verts.push_back(0.0f); verts.push_back(1.0f); verts.push_back(0.0f);
        verts.push_back((x / r + 1.0f) * 0.5f); verts.push_back((z / r + 1.0f) * 0.5f);
    }
    unsigned int centerTop = baseIndex;
    for (int i = 0; i < segments; ++i) {
        inds.push_back(centerTop);
        inds.push_back(centerTop + 1 + i + 1);
        inds.push_back(centerTop + 1 + i);
    }

    // create GL buffers
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    return VAO;
}

void renderRealisticCar(const Car& car, Shader& shader, GLuint cubeVAO, GLuint cylinderVAO)
{
    // Basic wrapper that matches main.cpp's complex implementation
    // For simplicity reuse main's render inlined here
    glm::mat4 model;
    float w = car.width; float h = car.height; float l = car.length;
    if (w <= 0.001f || h <= 0.001f || l <= 0.001f) return;
    float chassisH = h * 0.55f;
    float cabinH = h - chassisH;
    float hoodL = l * 0.20f; float cabinL = l * 0.55f; float trunkL = l - hoodL - cabinL;
    float yaw = 0.0f;
    if (glm::length(car.direction) > 0.001f) yaw = atan2(car.direction.x, car.direction.z);
    glm::mat4 baseModel = glm::mat4(1.0f);
    baseModel = glm::translate(baseModel, car.position);
    baseModel = glm::rotate(baseModel, yaw, glm::vec3(0, 1, 0));
    // chassis
    glm::vec3 chassisLocal = glm::vec3(0.0f, -h * 0.5f + chassisH * 0.5f, 0.0f);
    model = baseModel; model = glm::translate(model, chassisLocal); model = glm::scale(model, glm::vec3(w, chassisH, l));
    shader.SetMat4("model", model); shader.SetVec3("objectColor", car.color * 0.92f);
    glBindVertexArray(cubeVAO); glDrawArrays(GL_TRIANGLES, 0, 36);
    // cabin
    glm::vec3 cabinLocal = glm::vec3(0.0f, -h * 0.5f + chassisH + cabinH * 0.5f, 0.0f);
    model = baseModel; model = glm::translate(model, cabinLocal); model = glm::scale(model, glm::vec3(w * 0.88f, cabinH * 0.95f, cabinL * 0.98f));
    shader.SetMat4("model", model); shader.SetVec3("objectColor", car.color);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}
