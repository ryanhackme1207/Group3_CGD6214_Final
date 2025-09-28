#include "main.h"
#include "Shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

// Procedural terrain generation: create a heightfield grid in [-1,1]x[-1,1]
// Vertex format: position(3), normal(3), texcoord(2)
static int g_groundIndexCount = 0;

// Note: main.cpp scales the ground by 250 in X/Z when rendering. To avoid
// placing terrain bumps on top of road infrastructure we detect normalized
// positions that map to the main highways and return zero height there.
static float ProceduralHeight(float x, float z) {
    // x,z expected in [-1,1]
    // Map normalized coords to world-space (main.cpp scales by 250)
    const float WORLD_SCALE = 250.0f; // must match main.cpp scale
    float worldX = x * WORLD_SCALE;
    float worldZ = z * WORLD_SCALE;

    // Highway/sidewalk clearances (match values used in main.cpp)
    const float highwayHalf = 12.5f;   // half-width of main highway in meters
    const float sidewalkWidth = 2.5f;  // sidewalk depth
    const float sidewalkOffsetExtra = 0.3f; // additional offset used elsewhere
    const float clearance = highwayHalf + sidewalkWidth / 2.0f + sidewalkOffsetExtra; // ~15.3m

    // If the world-space position lies within the east-west or north-south highway
    // corridor, force height to zero so roads remain flat. This also implicitly
    // keeps sidewalks flat near roads.
    if (std::fabs(worldZ) < clearance || std::fabs(worldX) < clearance) {
        return 0.0f;
    }

    // Layered sinusoidal waves for gentle rolling terrain elsewhere
    float h = 0.0f;
    h += 0.12f * sinf(3.0f * x + 0.7f * z);
    h += 0.08f * sinf(6.0f * z - 0.5f * x);
    h += 0.06f * sinf((x + z) * 8.0f);
    h += 0.03f * sinf(20.0f * x) * cosf(18.0f * z);
    // small bump for variety
    h += 0.02f * sinf(40.0f * (x * x + z * z));
    return h;
}

int getGroundIndexCount() { return g_groundIndexCount; }

GLuint createGround()
{
    const int GRID = 128; // creates GRID x GRID vertices
    const float half = 1.0f;
    const float step = 2.0f / (GRID - 1);

    std::vector<float> verts;
    verts.reserve(GRID * GRID * 8);

    // first compute heights in a 2D array
    std::vector<float> heights(GRID * GRID);
    for (int z = 0; z < GRID; ++z) {
        for (int x = 0; x < GRID; ++x) {
            float fx = -half + x * step;
            float fz = -half + z * step;
            heights[z * GRID + x] = ProceduralHeight(fx, fz);
        }
    }

    // build vertex data with normals computed by central differences
    for (int z = 0; z < GRID; ++z) {
        for (int x = 0; x < GRID; ++x) {
            float fx = -half + x * step;
            float fz = -half + z * step;
            float h = heights[z * GRID + x];

            // approximate normal
            float hl = (x > 0) ? heights[z * GRID + (x-1)] : heights[z * GRID + x];
            float hr = (x + 1 < GRID) ? heights[z * GRID + (x+1)] : heights[z * GRID + x];
            float hd = (z > 0) ? heights[(z-1) * GRID + x] : heights[z * GRID + x];
            float hu = (z + 1 < GRID) ? heights[(z+1) * GRID + x] : heights[z * GRID + x];
            // derivative approximations
            glm::vec3 nx = glm::vec3(1.0f, (hl - hr) * 0.5f / step, 0.0f);
            glm::vec3 nz = glm::vec3(0.0f, (hd - hu) * 0.5f / step, 1.0f);
            glm::vec3 normal = glm::normalize(glm::cross(nz, nx));

            // texcoords tiled
            float u = (float)x / (GRID - 1) * 10.0f;
            float v = (float)z / (GRID - 1) * 10.0f;

            verts.push_back(fx); verts.push_back(h); verts.push_back(fz);
            verts.push_back(normal.x); verts.push_back(normal.y); verts.push_back(normal.z);
            verts.push_back(u); verts.push_back(v);
        }
    }

    std::vector<unsigned int> inds;
    inds.reserve((GRID - 1) * (GRID - 1) * 6);
    for (int z = 0; z < GRID - 1; ++z) {
        for (int x = 0; x < GRID - 1; ++x) {
            unsigned int i0 = z * GRID + x;
            unsigned int i1 = i0 + 1;
            unsigned int i2 = i0 + GRID;
            unsigned int i3 = i2 + 1;
            // two triangles
            inds.push_back(i0); inds.push_back(i2); inds.push_back(i1);
            inds.push_back(i1); inds.push_back(i2); inds.push_back(i3);
        }
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

    glBindVertexArray(0);

    g_groundIndexCount = static_cast<int>(inds.size());
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

// Simplified cylinder generator (lightweight, fewer segments) to avoid heavy code
GLuint createCylinder()
{
    const int segments = 8;
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    float h = 1.0f; float r = 0.5f;
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * 3.14159265f;
        float x = cosf(theta) * r;
        float z = sinf(theta) * r;
        // bottom
        verts.push_back(x); verts.push_back(-h/2.0f); verts.push_back(z);
        verts.push_back(0.0f); verts.push_back(-1.0f); verts.push_back(0.0f);
        verts.push_back((float)i/segments); verts.push_back(0.0f);
        // top
        verts.push_back(x); verts.push_back(h/2.0f); verts.push_back(z);
        verts.push_back(0.0f); verts.push_back(1.0f); verts.push_back(0.0f);
        verts.push_back((float)i/segments); verts.push_back(1.0f);
    }
    for (int i = 0; i < segments; ++i) {
        unsigned int a = i*2;
        unsigned int b = a+1;
        unsigned int c = (i+1)*2;
        unsigned int d = c+1;
        inds.push_back(a); inds.push_back(c); inds.push_back(b);
        inds.push_back(b); inds.push_back(c); inds.push_back(d);
    }
    // create GL buffers
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size()*sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
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
