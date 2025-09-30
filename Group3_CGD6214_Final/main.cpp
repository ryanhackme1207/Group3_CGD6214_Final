#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
using namespace glm;
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <random>
#include "Camera.h"
#include "Shader.h"
#include "Mesh.h"
#include "main.h"
#include "Model.h"

#include "SceneGraph.h"
#include "LODManager.h"
#include "SpatialPartition.h"

#include "Pedestrians.h"
#include "Traffic.h"
#include "DeferredRenderer.h"
#include "SimpleGUI.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//global state
bool useDirectionalLight = true;
bool lightKeyPressed = false;
int MSAA = 0;
bool msaaKeyPressed = false;
// Deferred rendering toggle
bool gUseDeferred = false; // removed static for GUI access
bool gDeferredKeyPressed = false; // removed static
DeferredRenderer gDeferred; // removed static

// Wireframe toggles
bool gWireframeAll = false;        // 1 toggles
bool gWireframeRoofs = false;      // 2 toggles (only roofs)
bool gWireframeBlock = false;      // 3 toggles (only the scene graph "Block" node)
SceneNode* gBlockNode = nullptr;   // assigned after creation

float timeOfDay = 12.0f;
const float DAY_CYCLE_DURATION = 60.0f;

// vertex & fragment shader (skybox)
const char* skyboxVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
)";

const char* skyboxFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform float timeOfDay;  
uniform float skyIntensity;  

void main() {
    vec4 texColor = texture(skybox, TexCoords);
    
    // Calculate darkness factor based on time of day
    float darknessFactor = 1.0;
    
    // Make sky darker during night (6pm - 6am)
    if (timeOfDay < 6.0 || timeOfDay > 18.0) {
        // At night, significantly reduce brightness (0.15 = very dark)
        darknessFactor = 0.15;
        
        // Add slightly more blue tint at night
        texColor.rgb = mix(texColor.rgb, vec3(0.1, 0.1, 0.2), 0.3);
    } else {
        // During day, use smooth transition based on time
        float dayProgress = (timeOfDay - 6.0) / 12.0;
        // Sine wave for smooth transition, adjusted for more contrast
        darknessFactor = 0.15 + (0.85 * skyIntensity * sin(dayProgress * 3.14159));
        
        // Add slight color adjustments for dawn/dusk
        if (timeOfDay < 8.0) { // Dawn
            texColor.rgb = mix(texColor.rgb, vec3(0.8, 0.6, 0.4), 0.2);
        } else if (timeOfDay > 16.0) { // Dusk
            texColor.rgb = mix(texColor.rgb, vec3(0.8, 0.5, 0.3), 0.2);
        }
    }
    
    // Apply darkness and slight desaturation at night
    vec3 finalColor = texColor.rgb * darknessFactor;
    if (timeOfDay < 6.0 || timeOfDay > 18.0) {
        float luminance = dot(finalColor, vec3(0.299, 0.587, 0.114));
        finalColor = mix(finalColor, vec3(luminance), 0.2); // Slight desaturation at night
    }
    
    FragColor = vec4(finalColor, texColor.a);
}
)";

// Window dimensions
const GLuint WIDTH = 1920, HEIGHT = 1080;

// Camera instance
Camera camera(glm::vec3(0.0f, 5.0f, 30.0f));  // Start further back to see the city

// Mouse variables
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Camera mode switching
bool cameraKeyPressed = false;

// Define storage for car vectors expected by Traffic.cpp
std::vector<Car> cars;
std::vector<Car> parkedCars; // parked cars in parking lots
float carSpawnTimer = 0.0f;

// Vertax (skybox)
float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

// texture (skybox)
unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        std::cout << "Loading cubemap texture: " << faces[i] << std::endl;
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        GLenum format = GL_RGB; // Default format
        if (data) {
            std::cout << "Success! Dimensions: " << width << "x" << height << ", Channels: " << nrChannels << std::endl;

            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;
            else
                format = GL_RGB; // fallback

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);

            unsigned char defaultData[3] = {
                (unsigned char)(i * 40),
                (unsigned char)((i + 2) * 40),
                (unsigned char)((i + 4) * 40)
            };
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, defaultData);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }

    return id;
}

unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

// Ensure prototypes are visible before main (duplicate-safe)
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
GLuint createCube();
// prototypes for helpers implemented in SceneHelpers.cpp
GLuint createGround();
GLuint createTriangularRoof();
GLuint createCylinder();
int getGroundIndexCount(); // prototype for procedural ground index count provided by SceneHelpers.cpp
// forward declare pool creation accessor
extern GLuint createPool(float width, float depth, int nx, int nz);
extern int getPoolIndexCount();

// Data structures for one-time city generation
struct BuildingInfo {
    glm::vec3 pos;
    float width, depth, height;
    glm::vec3 color;
    bool hasRoof;
    glm::vec3 roofColor;
    int buildingType;
};
struct SmallTreeInfo {
    glm::vec3 pos;
    float trunkH, trunkR, foliage;
};

// Generate city layout (fills buildings, smallTrees and buildingBoxes). Deterministic using given seed.
static void generateCity(std::vector<BuildingInfo>& buildings, std::vector<SmallTreeInfo>& smallTrees, std::vector<glm::vec4>& buildingBoxes, unsigned int seed = 12345u) {
    std::mt19937 rng(seed);
    auto rndf = [&](float a, float b) { return std::uniform_real_distribution<float>(a,b)(rng); };
    auto rndi = [&](int a, int b) { return std::uniform_int_distribution<int>(a,b)(rng); };

    buildings.clear(); smallTrees.clear(); buildingBoxes.clear();

    // Iconic towers (fixed)
    BuildingInfo kl; kl.pos = glm::vec3(25.0f, 0.0f, 25.0f); kl.width = 3.0f; kl.depth = 3.0f; kl.height = 421.0f; kl.color = glm::vec3(0.8f,0.8f,0.9f); kl.hasRoof = false; kl.buildingType = 4; buildings.push_back(kl);
    buildingBoxes.emplace_back(glm::vec4(25.0f, 25.0f, kl.width*0.5f, kl.height*0.5f));

    BuildingInfo eiff; eiff.pos = glm::vec3(-30.0f,0.0f,-30.0f); eiff.width = 12.0f; eiff.depth = 12.0f; eiff.height = 200.0f; eiff.color = glm::vec3(0.6f,0.5f,0.4f); eiff.hasRoof = false; eiff.buildingType = 3; buildings.push_back(eiff);
    buildingBoxes.emplace_back(glm::vec4(-30.0f,-30.0f,eiff.width*0.5f,eiff.height*0.5f));

    // Residential grid
    glm::vec3 mallCenter = glm::vec3(60.0f,0.0f,60.0f);
    for (int x = -100; x <= 100; x += 10) {
        for (int z = -100; z <= 100; z += 10) {
            if ((x >= -15 && x <= 15) || (z >= -15 && z <= 15)) continue;
            if (fmod(fabs((float)x), 40.0f) < 2.0f || fmod(fabs((float)z), 40.0f) < 2.0f) continue;
            float dxMall = x - 60.0f; float dzMall = z - 20.0f;
            if (dxMall * dxMall + dzMall * dzMall < 35.0f * 35.0f) continue;

            float actualX = x + rndf(-2.0f,2.0f);
            float actualZ = z + rndf(-2.0f,2.0f);

            float distCenter = sqrtf((float)(x*x + z*z));
            int buildingType = 0;
            int r = rndi(0,99);
            if (distCenter < 40.0f) {
                if (r < 20) buildingType = 4; else if (r < 55) buildingType = 3; else if (r < 80) buildingType = 2; else buildingType = 1;
            } else if (distCenter < 80.0f) {
                if (r < 10) buildingType = 4; else if (r < 40) buildingType = 3; else if (r < 75) buildingType = 2; else buildingType = 0;
            } else {
                if (r < 50) buildingType = 0; else if (r < 75) buildingType = 2; else buildingType = 1;
            }

            BuildingInfo b;
            b.buildingType = buildingType;
            b.hasRoof = false;
            switch (buildingType) {
            case 0: b.width = rndf(5.5f,10.5f); b.depth = rndf(6.5f,11.5f); b.height = rndf(4.5f,8.5f); b.hasRoof = (rndi(0,99) < 85); break;
            case 1: b.width = rndf(9.0f,15.0f); b.depth = rndf(7.0f,11.0f); b.height = rndf(6.0f,11.0f); b.hasRoof = (rndi(0,99) < 75); break;
            case 2: b.width = rndf(4.5f,7.5f); b.depth = rndf(8.5f,14.5f); b.height = rndf(7.0f,13.0f); b.hasRoof = (rndi(0,99) < 50); break;
            case 3: b.width = rndf(10.0f,18.0f); b.depth = rndf(9.0f,17.0f); b.height = rndf(10.0f,20.0f); b.hasRoof = (rndi(0,99) < 20); break;
            case 4: b.width = rndf(10.0f,36.0f); b.depth = rndf(10.0f,36.0f); b.height = rndf(30.0f,240.0f); b.hasRoof = (rndi(0,99) < 10); break;
            }
            b.pos = glm::vec3(actualX, 0.0f, actualZ);
            // color palette
            glm::vec3 houseColors[] = { glm::vec3(0.9f,0.9f,0.85f), glm::vec3(0.8f,0.7f,0.6f), glm::vec3(0.7f,0.6f,0.5f), glm::vec3(0.85f,0.8f,0.75f), glm::vec3(0.9f,0.85f,0.7f), glm::vec3(0.75f,0.7f,0.65f), glm::vec3(0.6f,0.5f,0.4f), glm::vec3(0.8f,0.75f,0.7f)};
            b.color = houseColors[rndi(0,7)];
            b.roofColor = glm::vec3(rndf(0.25f,0.5f), rndf(0.15f,0.35f), rndf(0.1f,0.3f));

            // clamp and register
            b.width = glm::min(b.width, 40.0f);
            b.depth = glm::min(b.depth, 40.0f);

            float halfW = b.width * 0.5f; float halfD = b.depth * 0.5f;

            // Avoid placing buildings too close to main highways (east-west at z=0, north-south at x=0)
            const float highwayHalf = 12.5f; // half-width of main highway
            const float roadClearance = 3.0f; // extra clearance from highway edge
            if (fabs(actualZ) - halfD < highwayHalf + roadClearance) continue;
            if (fabs(actualX) - halfW < highwayHalf + roadClearance) continue;

            // Avoid street light positions: lamps are at (i, +/-15) and (+/-15, i) for i = -120..120 step 30
            bool tooCloseLamp = false;
            const float lampClearance = 3.0f; // meters
            for (int li = -120; li <= 120 && !tooCloseLamp; li += 30) {
                for (int side = -1; side <= 1; side += 2) {
                    float lx = (float)li; float lz = side * 15.0f;
                    if (fabs(actualX - lx) < (halfW + lampClearance) && fabs(actualZ - lz) < (halfD + lampClearance)) { tooCloseLamp = true; break; }
                    // also the perpendicular lamps at x = +/-15
                    float lx2 = side * 15.0f; float lz2 = (float)li;
                    if ( fabs(actualX - lx2) < (halfW + lampClearance) && fabs(actualZ - lz2) < (halfD + lampClearance)) { tooCloseLamp = true; break; }
                }
            }
            if (tooCloseLamp) continue; // skip placement near lamps

            // Check collision with existing buildings to avoid overlap
            bool collidesWithExisting = false;
            for (const auto& bb : buildingBoxes) {
                float bx = bb.x, bz = bb.y, halfWb = bb.z, halfDb = bb.w;
                float minDistX = halfW + halfWb + 0.5f; // small buffer
                float minDistZ = halfD + halfDb + 0.5f;
                if (fabs(actualX - bx) < minDistX && fabs(actualZ - bz) < minDistZ) { collidesWithExisting = true; break; }
            }
            if (collidesWithExisting) continue; // skip placement if it would overlap

            // All checks passed: register building
            b.pos = glm::vec3(actualX, 0.0f, actualZ);
            buildings.push_back(b);
            buildingBoxes.emplace_back(glm::vec4(b.pos.x, b.pos.z, halfW, halfD));

            // small tree chance
            if (rndi(0,99) < 35) {
                SmallTreeInfo t;
                float angle = rndf(0.0f, 2.0f * 3.14159265f);
                float dist = halfW + 1.0f + rndf(0.0f,2.0f);
                t.pos = glm::vec3(b.pos.x + cos(angle)*dist, 0.0f, b.pos.z + sin(angle)*dist);
                if (fabs(t.pos.x) < 15.0f || fabs(t.pos.z) < 15.0f) { /*skip*/ }
                else {
                    t.trunkH = rndf(1.0f,1.8f);
                    t.trunkR = rndf(0.12f,0.3f);
                    t.foliage = rndf(0.8f,1.6f);
                    smallTrees.push_back(t);
                    buildingBoxes.emplace_back(glm::vec4(t.pos.x, t.pos.z, t.foliage*0.5f+0.4f, t.foliage*0.5f+0.4f));
                }
            }
        }
    }

    // CBD high-rises
    float cbdRadius = 35.0f;
    for (float gx=-cbdRadius; gx<=cbdRadius; gx+=6.0f) {
        for (float gz=-cbdRadius; gz<=cbdRadius; gz+=6.0f) {
            float dist = sqrt(gx*gx + gz*gz);
            if (dist > cbdRadius) continue;
            float bx = gx + rndf(-1.25f,1.25f);
            float bz = gz + rndf(-1.25f,1.25f);
            float bWidth = rndf(8.0f,30.0f);
            float bDepth = rndf(8.0f,30.0f);
            float bHeight = rndf(40.0f,320.0f);
            float halfW = bWidth*0.5f; float halfD = bDepth*0.5f;

            // Ensure buildings are not sitting on main highways: nudge them outside clearance instead of simply skipping
            const float highwayHalf = 12.5f; // half-width of main highways
            const float roadClearance = 4.0f; // desired clearance from highway edge
            // If building overlaps EW highway (z ~ 0), push it outward in Z
            if (fabs(bz) - halfD < highwayHalf + roadClearance) {
                if (bz >= 0.0f) bz = (highwayHalf + roadClearance) + halfD + 0.5f;
                else bz = -((highwayHalf + roadClearance) + halfD + 0.5f);
            }
            // If building overlaps NS highway (x ~ 0), push it outward in X
            if (fabs(bx) - halfW < highwayHalf + roadClearance) {
                if (bx >= 0.0f) bx = (highwayHalf + roadClearance) + halfW + 0.5f;
                else bx = -((highwayHalf + roadClearance) + halfW + 0.5f);
            }

            bool coll = false;
            for (auto &bb: buildingBoxes) {
                if (fabs(bx - bb.x) < (halfW + bb.z + 1.0f) && fabs(bz - bb.y) < (halfD + bb.w + 1.0f)) { coll=true; break;
}
            }
            if (coll) continue;
            BuildingInfo b; b.pos=glm::vec3(bx,0.0f,bz); b.width=bWidth; b.depth=bDepth; b.height=bHeight; b.color=glm::vec3(0.55f,0.58f,0.68f); b.buildingType=4; buildings.push_back(b);
            buildingBoxes.emplace_back(glm::vec4(bx,bz,halfW,halfD));
        }
    }

    // Commercial strips
    float avenues[2] = {40.0f, -40.0f};
    for (int ai=0; ai<2; ++ai) {
        float az = avenues[ai];
        for (float ax=-110.0f; ax<=110.0f; ax+=12.0f) {
            float bx = ax + rndf(-1.5f,1.5f);
            float bz = az + rndf(-1.0f,1.0f);
            float bWidth = rndf(10.0f,16.0f); float bDepth = rndf(6.0f,10.0f); float bHeight = rndf(8.0f,20.0f);
            float halfW = bWidth*0.5f, halfD = bDepth*0.5f;
            // Nudge commercial buildings away from highways if they overlap
            const float highwayHalfCom = 12.5f;
            const float roadClearanceCom = 3.5f;
            if (fabs(bz) - halfD < highwayHalfCom + roadClearanceCom) {
                if (bz >= 0.0f) bz = (highwayHalfCom + roadClearanceCom) + halfD + 0.5f;
                else bz = -((highwayHalfCom + roadClearanceCom) + halfD + 0.5f);
            }
            if (fabs(bx) - halfW < highwayHalfCom + roadClearanceCom) {
                if (bx >= 0.0f) bx = (highwayHalfCom + roadClearanceCom) + halfW + 0.5f;
                else bx = -((highwayHalfCom + roadClearanceCom) + halfW + 0.5f);
            }
            bool coll=false; for (auto &bb: buildingBoxes) { if (fabs(bx-bb.x)<(halfW+bb.z+0.8f) && fabs(bz-bb.y)<(halfD+bb.w+0.8f)) { coll=true; break; } }
            if (coll) continue;
            BuildingInfo b; b.pos=glm::vec3(bx,0.0f,bz); b.width=bWidth; b.depth=bDepth; b.height=bHeight; b.color=glm::vec3(0.78f,0.76f,0.72f); b.buildingType=3; buildings.push_back(b);
            buildingBoxes.emplace_back(glm::vec4(bx,bz,halfW,halfD));
        }
    }
}

static void drawCity(const std::vector<BuildingInfo>& buildings, const std::vector<SmallTreeInfo>& smallTrees, Shader& buildingShader, GLuint cubeVAO, GLuint roofVAO, GLuint cylinderVAO) {
    glm::mat4 model;
    const glm::vec3 camPos = camera.GetPosition();
    for (const auto &b : buildings) {
        float dist = glm::distance(camPos, glm::vec3(b.pos.x, 0.0f, b.pos.z));
        if (dist <= 100.0f) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(b.pos.x, b.height/2.0f, b.pos.z));
            model = glm::scale(model, glm::vec3(b.width, b.height, b.depth));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", b.color);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            if (b.hasRoof) {
                float roofHeight = glm::min(b.height*0.6f + 1.0f, 8.0f);
                float roofY = b.height + roofHeight*0.5f;
                glm::mat4 roofModel = glm::mat4(1.0f);
                roofModel = glm::translate(roofModel, glm::vec3(b.pos.x, roofY, b.pos.z));
                roofModel = glm::scale(roofModel, glm::vec3(b.width*1.02f, roofHeight, b.depth*1.02f));
                buildingShader.SetMat4("model", roofModel);
                buildingShader.SetVec3("objectColor", b.roofColor);
                if (gWireframeRoofs && !gWireframeAll) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glBindVertexArray(roofVAO);
                glDrawArrays(GL_TRIANGLES, 0, 18);
                if (gWireframeRoofs && !gWireframeAll) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        } else if (dist <= 200.0f) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(b.pos.x, (b.height*0.5f) / 1.5f, b.pos.z));
            model = glm::scale(model, glm::vec3(b.width * 0.9f, b.height * 0.66f, b.depth * 0.9f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", b.color * 0.85f);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        } else {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(b.pos.x, 1.0f, b.pos.z));
            model = glm::scale(model, glm::vec3(1.0f, 2.0f, 1.0f));
            buildingShader.SetMat4("model", model);
            glm::vec3 farColor = glm::vec3(0.5f) * glm::vec3(0.6f) + b.color * 0.1f;
            buildingShader.SetVec3("objectColor", farColor);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
    // small trees
    for (const auto &t : smallTrees) {
        float dist = glm::distance(camPos, glm::vec3(t.pos.x, 0.0f, t.pos.z));
        if (dist > 120.0f) continue; // skip distant small trees
        if (dist <= 60.0f) {
            // full detail: trunk + foliage
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(t.pos.x, t.trunkH/2.0f, t.pos.z));
            model = glm::scale(model, glm::vec3(t.trunkR, t.trunkH, t.trunkR));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.36f, 0.20f, 0.09f));
            glBindVertexArray(cylinderVAO);
            glDrawElements(GL_TRIANGLES, 16 * 12, GL_UNSIGNED_INT, 0);

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(t.pos.x, t.trunkH + t.foliage/2.0f, t.pos.z));
            model = glm::scale(model, glm::vec3(t.foliage, t.foliage, t.foliage));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.1f,0.55f,0.12f));
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        } else {
            // mid LOD: single small cube as foliage
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(t.pos.x, t.trunkH + t.foliage/2.0f, t.pos.z));
            model = glm::scale(model, glm::vec3(t.foliage * 0.75f, t.foliage * 0.75f, t.foliage * 0.75f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.12f,0.5f,0.11f));
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
}

// Ensure prototypes are visible before main (duplicate-safe)
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
GLuint createCube();
// prototypes for helpers implemented in SceneHelpers.cpp
GLuint createGround();
GLuint createTriangularRoof();
GLuint createCylinder();
int getGroundIndexCount(); // prototype for procedural ground index count provided by SceneHelpers.cpp
// forward declare pool creation accessor
extern GLuint createPool(float width, float depth, int nx, int nz);
extern int getPoolIndexCount();

int main()
{
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Create window
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Realistic Residential City", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialize GLEW
    if (glewInit() != GLEW_OK)
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Configure global OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    MSAA = 4;

    // Load shaders from files
    Shader buildingShader;
    if (!buildingShader.LoadFromFile("shaders/basic.vert", "shaders/lighting.frag")) {
        std::cout << "Failed to load shader files!" << std::endl;
        std::cout << "Make sure you have:" << std::endl;
        std::cout << "  - shaders/basic.vert" << std::endl;
        std::cout << "  - shaders/Lighting.frag" << std::endl;
        std::cout << "in your project directory" << std::endl;
        return -1;
    }
    // NEW: print the shader source file paths just loaded
    buildingShader.PrintSourceFilePaths();

    // VAO / VBO (skybox)
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // textures (skybox)
    std::vector<std::string> faces = {
        "Textures/px.jpg",
        "Textures/nx.jpg",
        "Textures/py.jpg",
        "Textures/ny.jpg",
        "Textures/pz.jpg",
        "Textures/nz.jpg"
    };
    unsigned int cubemapTexture = loadCubemap(faces);
    unsigned int skyboxShader = createShaderProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
    glUseProgram(skyboxShader);
    glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);

    std::cout << "Successfully loaded shaders from files!" << std::endl;

    // Ensure shader defaults to safe values and print active uniforms for debugging
    buildingShader.Use();
    buildingShader.SetFloat("bumpIntensity", 0.0f);
    buildingShader.SetFloat("time", 0.0f);
    buildingShader.SetBool("hasTexture", false);
    buildingShader.SetBool("useNormalMap", false);
    buildingShader.PrintActiveUniforms();

    // Create a small additive glow shader (used for second pass to add light glows)
    Shader glowShader;
    std::string glowVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // pass-through; no varying required for this simple shader
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

    std::string glowFrag = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 glowColor;
uniform float glowAlpha;
void main() {
    FragColor = vec4(glowColor, glowAlpha);
}
)";

    glowShader.LoadFromString(glowVert, glowFrag);

    // Spotlight volumetric beam shader (additive) - uses world-space position computed in vertex shader
    Shader spotlightShader;
    std::string spotVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 WorldPos;

void main() {
    vec4 world = model * vec4(aPos, 1.0);
    WorldPos = world.xyz;
    gl_Position = projection * view * world;
}
)";

    std::string spotFrag = R"(
#version 330 core
in vec3 WorldPos;
out vec4 FragColor;

uniform vec3 beamApex; // world-space apex position
uniform vec3 beamDir; // normalized direction the beam points toward (toward illuminated area)
uniform float beamLength;
uniform float beamRadius;
uniform vec3 beamColor;
uniform float beamIntensity; // scalar

void main() {
    // Vector from apex into beam
    vec3 L = WorldPos - beamApex;
    // project onto beam axis (beamDir points toward illuminated direction)
    float along = dot(L, -beamDir);
    if (along < 0.0 || along > beamLength) discard;

    // perpendicular distance from axis
    vec3 proj = -beamDir * along;
    vec3 perp = L - proj;
    float radial = length(perp);

    // radial falloff (strong near axis), and longitudinal falloff
    float radialFactor = 1.0 - smoothstep(0.0, beamRadius, radial);
    float longitudinal = 1.0 - (along / beamLength);
    float alpha = beamIntensity * radialFactor * longitudinal;
    alpha = clamp(alpha, 0.0, 1.0);

    // tiny soft edge by discarding very low alpha to save fillrate
    if (alpha < 0.005) discard;

    FragColor = vec4(beamColor * alpha, alpha);
}
)";

    spotlightShader.LoadFromString(spotVert, spotFrag);

    // Create geometry
    GLuint cubeVAO = createCube();
    GLuint groundVAO = createGround();
    GLuint roofVAO = createTriangularRoof();
    GLuint cylinderVAO = createCylinder();
    // create pool VAO
    GLuint poolVAO = createPool(12.0f, 8.0f, 64, 48);

    // Water shader
    Shader waterShader;
    if (!waterShader.LoadFromFile("shaders/water.vert", "shaders/water.frag")) {
        std::cerr << "Failed to load water shaders" << std::endl;
    }
    waterShader.Use();
    waterShader.SetInt("skybox", 0);
    // optional dudv map if provided

    // --- Shadow map setup ---
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    GLuint depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    // create depth texture
    GLuint depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Shadow framebuffer not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load shadow shader
    Shader depthShader;
    if (!depthShader.LoadFromFile("shaders/shadowmap.vert", "shaders/shadowmap.frag")) {
        std::cerr << "Failed to load shadowmap shaders" << std::endl;
    }

    // Prepare deterministic city data (generate once)
    std::vector<BuildingInfo> buildings;
    std::vector<SmallTreeInfo> smallTrees;
    std::vector<glm::vec4> buildingBoxes; // x,z,halfW,halfD
    generateCity(buildings, smallTrees, buildingBoxes, 12345u);

    // Attempt to place pool at user's requested camera location (if space allows)
    glm::vec3 requestedPoolPos = glm::vec3(-25.00f, 0.05f, 44.1449f);
    glm::vec3 poolPos = glm::vec3(-25.00f, 0.05f, 0.0f);
    bool poolPlaced = false;
    {
        float poolHalfW = 6.0f; // createPool used width=12
        float poolHalfD = 4.0f; // createPool used depth=8
        float safety = 1.5f; // extra clearance
        const float highwayHalf = 12.5f; // avoid main highways

        // First: try requested position if it is not colliding and not on road
        auto CollidesAny = [&](float cx, float cz)->bool {
            for (const auto &bb : buildingBoxes) {
                float bx = bb.x, bz = bb.y, halfB_W = bb.z, halfB_D = bb.w;
                if (fabs(cx - bx) < (poolHalfW + halfB_W + safety) && fabs(cz - bz) < (poolHalfD + halfB_D + safety)) return true;
            }
            // avoid main highways (x~0 or z~0 regions)
            if (fabs(cx) < (highwayHalf + poolHalfW + 1.0f) || fabs(cz) < (highwayHalf + poolHalfD + 1.0f)) return true;
            return false;
        };

        if (!CollidesAny(requestedPoolPos.x, requestedPoolPos.z)) {
            poolPos = requestedPoolPos;
            poolPlaced = true;
        }

        // Second: try placing pool adjacent to existing buildings (four sides)
        if (!poolPlaced) {
            for (const auto &bb : buildingBoxes) {
                if (poolPlaced) break;
                float bx = bb.x, bz = bb.y, halfBW = bb.z, halfBD = bb.w;
                // sample candidates along each side at several offsets
                const int samples = 6;
                // Right side (positive X)
                for (int s = 0; s < samples && !poolPlaced; ++s) {
                    float frac = (samples == 1) ? 0.5f : (float)s / (float)(samples-1);
                    float cz = bz - halfBD + frac * (halfBD * 2.0f);
                    float cx = bx + halfBW + poolHalfW + safety + 0.5f;
                    if (!CollidesAny(cx, cz)) { poolPos = glm::vec3(cx, requestedPoolPos.y, cz); poolPlaced = true; break;
}
                }
                // Left side (negative X)
                for (int s = 0; s < samples && !poolPlaced; ++s) {
                    float frac = (samples == 1) ? 0.5f : (float)s / (float)(samples-1);
                    float cz = bz - halfBD + frac * (halfBD * 2.0f);
                    float cx = bx - halfBW - poolHalfW - safety - 0.5f;
                    if (!CollidesAny(cx, cz)) { poolPos = glm::vec3(cx, requestedPoolPos.y, cz); poolPlaced = true; break;
}
                }
                // Front side (positive Z)
                for (int s = 0; s < samples && !poolPlaced; ++s) {
                    float frac = (samples == 1) ? 0.5f : (float)s / (float)(samples-1);
                    float cx = bx - halfBW + frac * (halfBW * 2.0f);
                    float cz = bz + halfBD + poolHalfD + safety + 0.5f;
                    if (!CollidesAny(cx, cz)) { poolPos = glm::vec3(cx, requestedPoolPos.y, cz); poolPlaced = true; break;
}
                }
                // Back side (negative Z)
                for (int s = 0; s < samples && !poolPlaced; ++s) {
                    float frac = (samples == 1) ? 0.5f : (float)s / (float)(samples-1);
                    float cx = bx - halfBW + frac * (halfBW * 2.0f);
                    float cz = bz - halfBD - poolHalfD - safety - 0.5f;
                    if (!CollidesAny(cx, cz)) { poolPos = glm::vec3(cx, requestedPoolPos.y, cz); poolPlaced = true; break;
}
                }
            }
        }

        // Last fallback: grid search like before but skip roads and buildings
        if (!poolPlaced) {
            for (float px = -100.0f; px <= 100.0f && !poolPlaced; px += 5.0f) {
                for (float pz = -100.0f; pz <= 100.0f && !poolPlaced; pz += 5.0f) {
                    if (CollidesAny(px, pz)) continue;
                    poolPos = glm::vec3(px, requestedPoolPos.y, pz);
                    poolPlaced = true;
                    break;
                }
            }
            if (!poolPlaced) {
                poolPos = glm::vec3(50.0f, requestedPoolPos.y, 50.0f);
            }
        }
    }

    // Load external model: visiongt1.obj (try several likely paths). Model loader will parse MTL and apply textures if available.
    Model visionModel;
    bool visionLoaded = false;
    std::vector<std::string> visionPaths = {
        "3D/source/visiongt1.obj",
        
    };
    for (const auto &p : visionPaths) {
        if (visionModel.LoadOBJ(p)) { visionLoaded = true; std::cout << "vision model loaded from: " << p << std::endl; break; }
    }
    if (!visionLoaded) {
        std::cout << "Warning: visiongt1.obj not found. Place visiongt1.obj (and visiongt.mlt/associated textures) in project folder or Models/." << std::endl;
    }

    // Free-space placement for visiongt model: park beside the highway (Bugatti showcase)
    // We'll search sidewalks along the main east-west (z=0) and north-south (x=0) highways
    glm::vec3 visionPosition = glm::vec3(0.0f);
    float visionYaw = 0.0f; // rotation around Y
    const float visionScale = 0.02f; // requested scale for the model

    // Highway/sidewalk geometry must match rendering code above
    const float highwayHalf = 12.5f; // half-width of highway
    const float sidewalkWidth = 2.5f;
    const float sidewalkOffsetZ = highwayHalf + sidewalkWidth / 2.0f + 0.3f; // z offset for EW sidewalks
    const float sidewalkOffsetX = highwayHalf + sidewalkWidth / 2.0f + 0.3f; // x offset for NS sidewalks

    bool placed = false;
    float placeY_onSidewalk = 0.03f; // small Y so car sits on road/sidewalk
    float collisionBuffer = 2.0f; // meters of clearance when testing building boxes

    // Try east-west highway sidewalks first (iterate X positions)
    for (int x = -100; x <= 100 && !placed; x += 5) {
        for (int side = -1; side <= 1 && !placed; side += 2) {
            glm::vec3 cand((float)x, placeY_onSidewalk, side * sidewalkOffsetZ);
            bool coll = false;
            for (const auto &bb : buildingBoxes) {
                float bx = bb.x, bz = bb.y, halfBW = bb.z, halfBD = bb.w;
                if (fabs(cand.x - bx) < (halfBW + collisionBuffer) && fabs(cand.z - bz) < (halfBD + collisionBuffer)) { coll = true; break; }
            }
            if (!coll) {
                visionPosition = cand;
                // orient along X axis (east-west). If side>0 place facing +X, else -X
                visionYaw = (side > 0) ? glm::radians(90.0f) : glm::radians(-90.0f);
                placed = true;
            }
        }
    }

    // If not placed, try north-south highway sidewalks (iterate Z positions)
    for (int z = -100; z <= 100 && !placed; z += 5) {
        for (int side = -1; side <= 1 && !placed; side += 2) {
            glm::vec3 cand(side * sidewalkOffsetX, placeY_onSidewalk, (float)z);
            bool coll = false;
            for (const auto &bb : buildingBoxes) {
                float bx = bb.x, bz = bb.y, halfBW = bb.z, halfBD = bb.w;
                if (fabs(cand.x - bx) < (halfBW + collisionBuffer) && fabs(cand.z - bz) < (halfBD + collisionBuffer)) { coll = true; break; }
            }
            if (!coll) {
                visionPosition = cand;
                // orient along Z axis (north-south). If side>0 place facing -Z, else +Z (so car points along road)
                visionYaw = (side > 0) ? glm::radians(180.0f) : glm::radians(0.0f);
                placed = true;
            }
        }
    }

    // Final fallback
    if (!placed) {
        visionPosition = glm::vec3(-90.0f, 1.0f, -90.0f);
        visionYaw = 0.0f;
    }

    // Build a small scene graph to provide a multi-level hierarchical model
    SceneGraph sceneGraph;
    auto rootNode = sceneGraph.GetRoot();
    // Register root with GUI for hierarchy browser
    SimpleGUI::Instance().SetSceneRoot(rootNode.get());

    // Create simple reusable cube mesh for scene graph nodes
    auto cubeMesh = std::make_shared<Mesh>(Mesh::CreateCube());

    // Level 1: District
    auto district = std::make_shared<SceneNode>("District");
    glm::mat4 distTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-40.0f, 0.0f, -40.0f));
    district->SetLocalTransform(distTransform);
    rootNode->AddChild(district);

    // Level 2: Block (child of district)
    auto block = std::make_shared<SceneNode>("Block");
    glm::mat4 blockTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    block->SetLocalTransform(blockTransform);
    district->AddChild(block);
    gBlockNode = block.get();

    // Level 3: House (child of block)
    auto house = std::make_shared<SceneNode>("House");
    glm::mat4 houseTransform = glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 2.0f, 6.0f));
    houseTransform = glm::scale(houseTransform, glm::vec3(6.0f, 4.0f, 5.0f));
    house->SetLocalTransform(houseTransform);
    house->SetMesh(cubeMesh);
    block->AddChild(house);

    // Level 4: Roof (child of house)
    auto roofNode = std::make_shared<SceneNode>("Roof");
    glm::mat4 roofTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    roofTransform = glm::scale(roofTransform, glm::vec3(1.05f, 0.3f, 1.05f));
    roofNode->SetLocalTransform(roofTransform);
    // Use cube mesh scaled thin to approximate roof
    roofNode->SetMesh(cubeMesh);
    house->AddChild(roofNode);

    // Level 5: Window (child of roof) to satisfy >=4 levels beyond root
    auto windowNode = std::make_shared<SceneNode>("Window");
    glm::mat4 winTransform = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, -0.8f, 2.6f));
    winTransform = glm::scale(winTransform, glm::vec3(1.2f, 1.0f, 0.05f));
    windowNode->SetLocalTransform(winTransform);
    windowNode->SetMesh(cubeMesh);
    roofNode->AddChild(windowNode);

    // Populate parked cars for the shopping mall parking lot
    // Mall center at (60, 0, 60)
    srand(424242); // fixed seed for consistent parked car placement
    glm::vec3 mallCenter = glm::vec3(60.0f, 0.0f, 60.0f);
    //    int rows = 4;
    //    int cols = 10;
    //    float spacingX = 4.5f; // space between parking spaces
    //    float spacingZ = 6.0f; // aisle spacing
    //    float startX = mallCenter.x - (cols / 2.0f - 0.5f) * spacingX;
    //    float startZ = mallCenter.z - (rows / 2.0f - 0.5f) * spacingZ - 30.0f; // place parking in front of mall
    //    for (int r = 0; r < rows; ++r) {
    //        for (int c = 0; c < cols; ++c) {
    //            float px = startX + c * spacingX + ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
    //            float pz = startZ + r * spacingZ + ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
    //            glm::vec3 pos = glm::vec3(px, 0.85f, pz);
    // Use same parking layout as renderShoppingMallComplex to avoid placing cars on highways
    float mallWidth = 60.0f;
    float mallDepth = 40.0f;
    float parkingWidth = mallWidth + 20.0f; // 80
    float parkingDepth = 30.0f;
    glm::vec3 parkingCenter = glm::vec3(mallCenter.x, 0.01f, mallCenter.z - mallDepth / 2.0f - 10.0f);

    int cols = 10; // spaces per row
    int rows = 4;
    float spaceWidth = (parkingWidth - 10.0f) / (float)cols;
    float startLineX = parkingCenter.x - parkingWidth / 2.0f + 5.0f + spaceWidth / 2.0f;
    float firstLineZ = parkingCenter.z - parkingDepth / 2.0f + 2.0f;
    float rowSpacing = 6.0f; // match previous spacingZ approx

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float px = startLineX + c * spaceWidth + ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
            float pz = firstLineZ + r * rowSpacing + ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
            glm::vec3 pos = glm::vec3(px, 0.85f, pz);
            int colorIndex = rand() % 10;
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
            int carType = rand() % 4;
            // parked cars have speed 0
            parkedCars.emplace_back(pos, glm::vec3(0.0f), carColors[colorIndex], 0.0f, 0, 0, carType);
            // Ensure parked car has physical dimensions so it will be rendered by renderRealisticCar
            {
                Car &pc = parkedCars.back();
                // Assign reasonable default dimensions (meters) and vary slightly by carType
                float baseWidth = 1.8f;
                float baseHeight = 1.25f;
                float baseLength = 3.8f;
                switch (carType) {
                case 0: baseWidth = 1.7f; baseHeight = 1.2f; baseLength = 3.6f; break; // small car
                case 1: baseWidth = 1.9f; baseHeight = 1.3f; baseLength = 4.2f; break; // sedan
                case 2: baseWidth = 2.0f; baseHeight = 1.4f; baseLength = 4.6f; break; // suv
                case 3: baseWidth = 1.6f; baseHeight = 1.15f; baseLength = 3.2f; break; // compact
                }
                // slight random jitter so parked cars look varied
                float jitterScale = ((rand() % 100) / 100.0f - 0.5f) * 0.08f;
                pc.width = baseWidth * (1.0f + jitterScale);
                pc.height = baseHeight * (1.0f + jitterScale * 0.5f);
                pc.length = baseLength * (1.0f + jitterScale);
            }
        }
    }

    // Spawn pedestrians after parked cars are populated
    spawnPedestrians(40); // spawn 40 pedestrians

    // Set up camera boundaries for the larger city with tall buildings
    camera.SetBoundaries(-120.0f, 120.0f, -120.0f, 120.0f, 2.0f, 300.0f);  // Higher ceiling for tall buildings
    camera.SetCollisionRadius(2.0f);
    camera.EnableSmoothMovement(false, 8.0f);

    // Print controls
    std::cout << "=== CAMERA CONTROLS ===" << std::endl;
    std::cout << "WASD: Move camera" << std::endl;
    std::cout << "Mouse: Look around" << std::endl;
    std::cout << "Q/E: Move up/down" << std::endl;
    std::cout << "C: Switch camera mode" << std::endl;
    std::cout << "R: Reset camera" << std::endl;
    std::cout << "Scroll: Zoom in/out" << std::endl;
    std::cout << "F5: Reload shaders" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
    std::cout << "=======================" << std::endl;
    std::cout << "=== LIGHT CONTROLS ===" << std::endl;
    std::cout << "L: Change current light mode" << std::endl;
    std::cout << "=======================" << std::endl;
    std::cout << "=== MSAA CONTROLS ===" << std::endl;
    std::cout << "M: Toggle MSAA on/off (Current: " << (MSAA > 0 ? "ON" : "OFF") << ")" << std::endl;
    std::cout << "=======================" << std::endl;


    if (MSAA > 0) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize GUI after GL context + glew
    SimpleGUI::Instance().Initialize(window);

    // Render loop
    glm::mat4 model;
    // Ensure buildings are generated only once to avoid sizes/positions changing while moving the camera
    bool buildingsGenerated = false;
    while (!glfwWindowShouldClose(window))
    {
        // Start GUI frame early so widgets affect this frame's rendering
        SimpleGUI::Instance().BeginFrame();
        // Per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Update day-night cycle
        timeOfDay += (24.0f / DAY_CYCLE_DURATION) * deltaTime;
        if (timeOfDay >= 24.0f) {
            timeOfDay = 0.0f;
        }
        // update simulation (cars, pedestrians, camera)
        carSpawnTimer += deltaTime;
        if (carSpawnTimer > 1.5f) { spawnCar(); carSpawnTimer = 0.0f; }
        updateCars(deltaTime);
        updatePedestrians(deltaTime);
        camera.UpdateSmoothMovement(deltaTime);
        camera.UpdateOrbitalCamera(deltaTime);
        camera.UpdateTransition(deltaTime);
        camera.UpdateFirstPerson(deltaTime); // update jump/gravity if in first-person

        // Compute skyIntensity (shared for both forward & deferred paths)
        float skyIntensity;
        if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) {
            float dayProgress = (timeOfDay - 6.0f) / 12.0f;
            skyIntensity = 0.6f + 0.2f * sin(dayProgress * M_PI);
            glClearColor(0.6f * skyIntensity, 0.8f * skyIntensity, 1.0f * skyIntensity, 1.0f);
        } else {
            skyIntensity = 0.1f;
            glClearColor(0.05f, 0.07f, 0.13f, 1.0f);
        }

        if (gUseDeferred) {
            // Ensure initialized (in case toggled before context established)
            // (Already initialized on toggle; extra safety)
            // Geometry pass
            gDeferred.BeginGeometryPass();
            Shader &geom = gDeferred.GetGeometryShader();
            glm::mat4 projection = camera.GetProjectionMatrix((float)WIDTH / (float)HEIGHT);
            glm::mat4 view = camera.GetViewMatrix();
            geom.SetMat4("projection", projection);
            geom.SetMat4("view", view);
            // Draw opaque scene (subset sufficient to demonstrate MRT)
            // Ground
            geom.SetBool("isGround", true);
            glm::mat4 modelM = glm::mat4(1.0f); modelM = glm::scale(modelM, glm::vec3(250.0f,1.0f,250.0f));
            geom.SetMat4("model", modelM);
            geom.SetVec3("objectColor", glm::vec3(0.4f,0.6f,0.3f));
            glBindVertexArray(groundVAO);
            glDrawElements(GL_TRIANGLES, getGroundIndexCount(), GL_UNSIGNED_INT, 0);
            geom.SetBool("isGround", false);
            // Buildings & trees (reuse existing helper)
            drawCity(buildings, smallTrees, geom, cubeVAO, roofVAO, cylinderVAO);
            // Iconic towers (simplified)
            modelM = glm::mat4(1.0f);
            modelM = glm::translate(modelM, glm::vec3(25.0f,210.5f,25.0f));
            modelM = glm::scale(modelM, glm::vec3(3.0f,421.0f,3.0f));
            geom.SetMat4("model", modelM);
            geom.SetVec3("objectColor", glm::vec3(0.8f,0.8f,0.9f));
            glBindVertexArray(cubeVAO); glDrawArrays(GL_TRIANGLES,0,36);
            modelM = glm::mat4(1.0f);
            modelM = glm::translate(modelM, glm::vec3(-30.0f,50.0f,-30.0f));
            modelM = glm::scale(modelM, glm::vec3(12.0f,100.0f,12.0f));
            geom.SetMat4("model", modelM);
            geom.SetVec3("objectColor", glm::vec3(0.6f,0.5f,0.4f));
            glDrawArrays(GL_TRIANGLES,0,36);
            // Roads (simple large quads)
            for (int x=-120; x<=120; x+=5){
                modelM = glm::mat4(1.0f); modelM = glm::translate(modelM, glm::vec3(x,0.02f,0.0f));
                modelM = glm::scale(modelM, glm::vec3(5.0f,0.04f,25.0f));
                geom.SetMat4("model", modelM); geom.SetVec3("objectColor", glm::vec3(0.25f));
                glBindVertexArray(cubeVAO); glDrawArrays(GL_TRIANGLES,0,36);
            }
            // Scene graph example hierarchy
            sceneGraph.Draw(geom);
            gDeferred.EndGeometryPass();

            // Lighting pass
            gDeferred.BeginLightingPass();
            Shader &lightPass = gDeferred.GetLightingShader();
            lightPass.SetVec3("viewPos", camera.GetPosition());
            // Dynamic ambient based on time of day
            glm::vec3 ambient = (timeOfDay>=6.0f && timeOfDay<=18.0f)? glm::vec3(0.25f) : glm::vec3(0.08f,0.08f,0.12f);
            lightPass.SetVec3("ambientColor", ambient);
            gDeferred.RenderQuad();
            gDeferred.EndLightingPass();

            // Skybox after lighting (depth already cleared in lighting pass so draw behind)
            glDepthFunc(GL_LEQUAL);
            glUseProgram(skyboxShader);
            glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
            glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"),1,GL_FALSE,glm::value_ptr(skyboxView));
            glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"),1,GL_FALSE,glm::value_ptr(projection));
            glUniform1f(glGetUniformLocation(skyboxShader, "timeOfDay"), timeOfDay);
            glUniform1f(glGetUniformLocation(skyboxShader, "skyIntensity"), skyIntensity);
            glBindVertexArray(skyboxVAO); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture); glDrawArrays(GL_TRIANGLES,0,36); glBindVertexArray(0); glDepthFunc(GL_LESS);

            // Render GUI last
            SimpleGUI::Instance().Draw(deltaTime);
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }
        // -------- Forward rendering path (existing) --------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        buildingShader.Use();
        buildingShader.SetFloat("bumpIntensity", 0.3f);
        buildingShader.SetFloat("time", (float)glfwGetTime());
        buildingShader.SetBool("hasTexture", false);
        buildingShader.SetBool("useNormalMap", false);
        glm::mat4 projection = camera.GetProjectionMatrix((float)WIDTH / (float)HEIGHT);
        glm::mat4 view = camera.GetViewMatrix();
        buildingShader.SetMat4("projection", projection);
        buildingShader.SetMat4("view", view);

        // Draw scene graph hierarchical models (District->Block->House->Roof->Window)
        sceneGraph.Draw(buildingShader);
        if (gWireframeBlock && !gWireframeAll && gBlockNode) {
            // Draw block node again in wireframe with depth func LEQUAL so lines overlay
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            GLint prevDepthFunc; glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
            glDepthFunc(GL_LEQUAL);
            gBlockNode->Draw(buildingShader);
            glDepthFunc(prevDepthFunc);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // Set lighting uniforms (sun-like lighting)
        // Predeclare light containers so we can use them for a second additive pass (glow)
        std::vector<glm::vec3> pointPositions;
        std::vector<glm::vec3> pointColors;
        std::vector<float> pointIntensities;
        std::vector<glm::vec3> spotPositions;
        std::vector<glm::vec3> spotDirections;
        std::vector<glm::vec3> spotColors;
        std::vector<float> spotIntensities;
        std::vector<float> spotInner;
        std::vector<float> spotOuter;

        if (useDirectionalLight) {
            // Directional light
            // Calculate sun position based on time of day
            float sunAngle = (timeOfDay / 24.0f) * 2.0f * M_PI;
            float sunHeight = sin(sunAngle);
            float sunHorizontal = cos(sunAngle);

            glm::vec3 sunPosition = glm::vec3(
                sunHorizontal * 100.0f,
                sunHeight * 100.0f,
                0.0f
            );

            // Calculate light color based on time of day
            float lightIntensity;
            glm::vec3 lightColor;
            glm::vec3 ambientColor;

            if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) {
                float noonFactor = 1.0f - abs(timeOfDay - 12.0f) / 6.0f;
                lightIntensity = (0.7f + 0.3f * noonFactor) * 1.5f; // Increased sun strength
                float morningBoost = 1.0f;
                if (timeOfDay < 10.0f) {
                    morningBoost = 1.25f - 0.05f * (10.0f - timeOfDay); // 1.25 at 6am, 1.05 at 10am
                }
                if (timeOfDay < 10.0f) {
                    lightColor = glm::vec3(1.0f, 0.93f, 0.8f) * lightIntensity * morningBoost;
                    ambientColor = glm::vec3(0.38f, 0.45f, 0.55f) * lightIntensity * 0.38f * morningBoost;
                }
                else if (timeOfDay > 14.0f) {
                    lightColor = glm::vec3(1.0f, 0.8f, 0.6f) * lightIntensity;
                    ambientColor = glm::vec3(0.3f, 0.4f, 0.5f) * lightIntensity * 0.3f;
                }
                else {
                    lightColor = glm::vec3(1.0f, 0.95f, 0.9f) * lightIntensity;
                    ambientColor = glm::vec3(0.3f, 0.4f, 0.5f) * lightIntensity * 0.3f;
                }
            }
            else {
                float nightIntensity = 0.1f + 0.05f * (1.0f - abs(timeOfDay - 0.0f) / 6.0f);
                lightColor = glm::vec3(0.7f, 0.8f, 1.0f) * nightIntensity;
                ambientColor = glm::vec3(0.1f, 0.1f, 0.2f) * 0.2f;
            }

            if (timeOfDay > 17.0f && timeOfDay < 19.0f) {
                float transition = 1.0f - abs(timeOfDay - 18.0f);
                lightColor = glm::mix(lightColor, glm::vec3(1.0f, 0.6f, 0.4f), transition);
                ambientColor = glm::mix(ambientColor, glm::vec3(0.4f, 0.3f, 0.2f), transition);
            }
            else if (timeOfDay > 5.0f && timeOfDay < 7.0f) {
                float transition = 1.0f - abs(timeOfDay - 6.0f);
                lightColor = glm::mix(lightColor, glm::vec3(1.0f, 0.7f, 0.5f), transition);
                ambientColor = glm::mix(ambientColor, glm::vec3(0.3f, 0.3f, 0.4f), transition);
            }

            buildingShader.SetVec3("lightDir", -glm::normalize(sunPosition));
            buildingShader.SetVec3("lightColor", lightColor);
            buildingShader.SetVec3("ambientColor", ambientColor);

            // Setup street lamp point lights (they turn on at night)
            // Compute a lightScale based on skyIntensity so lights fade in at dusk
            float lightScale = glm::clamp(1.0f - (skyIntensity - 0.1f) / 0.7f, 0.0f, 1.0f);

            // East-West street lights
            for (int i = -120; i <= 120; i += 30) {
                for (int side = -1; side <= 1; side += 2) {
                    float zPos = side * 15.0f;
                    pointPositions.emplace_back(glm::vec3(i, 9.5f, zPos));
                    pointColors.emplace_back(glm::vec3(1.0f, 0.95f, 0.8f));
                    pointIntensities.emplace_back(4.0f * lightScale);
                }
            }
            // North-South street lights
            for (int i = -120; i <= 120; i += 30) {
                if (i >= -15 && i <= 15) continue;
                for (int side = -1; side <= 1; side += 2) {
                    float xPos = side * 15.0f;
                    pointPositions.emplace_back(glm::vec3(xPos, 9.5f, i));
                    pointColors.emplace_back(glm::vec3(1.0f, 0.95f, 0.8f));
                    pointIntensities.emplace_back(4.0f * lightScale);
                }
            }

            // Add small warm point-lights on buildings to make them glow at night
            // Keep total point lights within shader capacity (64). Add lights only at night via lightScale multiplier.
            size_t maxPointLights = 64;
            // reserve some slots for other lights (spotlights etc) - we'll allow up to maxPointLights
            for (const auto &b : buildings) {
                if (pointPositions.size() >= maxPointLights) break;
                // place light slightly above roof center
                glm::vec3 lp = glm::vec3(b.pos.x, b.height + 2.0f, b.pos.z);
                // simple visibility filter: skip extremely tall towers (they already have spotlights)
                if (b.height > 100.0f) continue;
                pointPositions.push_back(lp);
                // warm building light color
                pointColors.emplace_back(glm::vec3(1.0f, 0.85f, 0.55f));
                // intensity scaled by time-of-day (lightScale) so they glow only at night
                pointIntensities.emplace_back(2.2f * lightScale);
            }

            int numPL = (int)pointPositions.size();
            buildingShader.SetInt("numPointLights", numPL);
            for (int i = 0; i < numPL; ++i) {
                std::string base = "pointLights[" + std::to_string(i) + "]";
                buildingShader.SetVec3(base + ".position", pointPositions[i]);
                buildingShader.SetVec3(base + ".color", pointColors[i]);
                buildingShader.SetFloat(base + ".intensity", pointIntensities[i]);
                buildingShader.SetFloat(base + ".constant", 1.0f);
                buildingShader.SetFloat(base + ".linear", 0.09f);
                buildingShader.SetFloat(base + ".quadratic", 0.032f);
            }

            // Setup spotlights on tall buildings (KL Tower, Eiffel Tower)
            // KL Tower spotlight (points downward)
            glm::vec3 klPos = glm::vec3(25.0f, 421.0f + 30.0f, 25.0f);
            spotPositions.push_back(klPos);
            spotDirections.push_back(glm::vec3(0.0f, -1.0f, 0.0f));
            spotColors.push_back(glm::vec3(1.0f, 0.98f, 0.9f));
            spotIntensities.push_back(8.0f * lightScale);
            spotInner.push_back(cos(glm::radians(12.5f)));
            spotOuter.push_back(cos(glm::radians(20.0f)));

            // Eiffel Tower spotlight (small angled downward spotlight)
            glm::vec3 eiffPos = glm::vec3(-30.0f, 315.0f + 15.0f, -30.0f);
            spotPositions.push_back(eiffPos);
            spotDirections.push_back(glm::normalize(glm::vec3(0.1f, -1.0f, 0.05f)));
            spotColors.push_back(glm::vec3(1.0f, 0.95f, 0.9f));
            spotIntensities.push_back(6.0f * lightScale);
            spotInner.push_back(cos(glm::radians(10.0f)));
            spotOuter.push_back(cos(glm::radians(18.0f)));

            int numSL = (int)spotPositions.size();
            buildingShader.SetInt("numSpotLights", numSL);
            for (int i = 0; i < numSL; ++i) {
                std::string base = "spotLights[" + std::to_string(i) + "]";
                buildingShader.SetVec3(base + ".position", spotPositions[i]);
                buildingShader.SetVec3(base + ".direction", spotDirections[i]);
                buildingShader.SetVec3(base + ".color", spotColors[i]);
                buildingShader.SetFloat(base + ".intensity", spotIntensities[i]);
                buildingShader.SetFloat(base + ".innerCutoff", spotInner[i]);
                buildingShader.SetFloat(base + ".outerCutoff", spotOuter[i]);
                buildingShader.SetFloat(base + ".constant", 1.0f);
                buildingShader.SetFloat(base + ".linear", 0.09f);
                buildingShader.SetFloat(base + ".quadratic", 0.032f);
            }

            // --- SHADOW PASS for primary spotlight (index 0) ---
            if (!spotPositions.empty()) {
                // compute light-space matrix for spotlight 0
                glm::vec3 lightPos = spotPositions[0];
                glm::vec3 lightDirVec = glm::normalize(spotDirections[0]);
                // choose up vector
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (fabs(glm::dot(up, lightDirVec)) > 0.99f) up = glm::vec3(1.0f, 0.0f, 0.0f);
                glm::mat4 lightView = glm::lookAt(lightPos, lightPos + (-lightDirVec), up);
                float near_plane = 1.0f;
                float far_plane = 800.0f;
                glm::mat4 lightProjection = glm::perspective(glm::radians(45.0f), 1.0f, near_plane, far_plane);
                glm::mat4 lightSpaceMatrix = lightProjection * lightView;

                // Render scene to depth map
                glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
                glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
                glClear(GL_DEPTH_BUFFER_BIT);

                depthShader.Use();
                depthShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);

                // Draw scene geometry using depthShader (reuse existing draw functions by passing depthShader)
                // scene graph
                sceneGraph.Draw(depthShader);
                // buildings
                drawCity(buildings, smallTrees, depthShader, cubeVAO, roofVAO, cylinderVAO);
                // ground
                {
                    glm::mat4 m = glm::mat4(1.0f);
                    m = glm::scale(m, glm::vec3(250.0f, 1.0f, 250.0f));
                    depthShader.SetMat4("model", m);
                    glBindVertexArray(groundVAO);
                    glDrawElements(GL_TRIANGLES, getGroundIndexCount(), GL_UNSIGNED_INT, 0);
                }
                // main towers and mall (large objects)
                // KL Tower
                {
                    glm::mat4 m = glm::mat4(1.0f);
                    m = glm::translate(m, glm::vec3(25.0f, 210.5f, 25.0f));
                    m = glm::scale(m, glm::vec3(3.0f, 421.0f, 3.0f));
                    depthShader.SetMat4("model", m);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                // Eiffel Tower
                {
                    glm::mat4 m = glm::mat4(1.0f);
                    m = glm::translate(m, glm::vec3(-30.0f, 50.0f, -30.0f));
                    m = glm::scale(m, glm::vec3(12.0f, 100.0f, 12.0f));
                    depthShader.SetMat4("model", m);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                // shopping mall
                renderShoppingMallComplex(depthShader, cubeVAO, cylinderVAO);

                // moving cars and pedestrians (render all for accurate shadows)
                for (const auto &c : cars) renderRealisticCar(c, depthShader, cubeVAO, cylinderVAO);
                for (const auto &pc : parkedCars) renderRealisticCar(pc, depthShader, cubeVAO, cylinderVAO);
                for (const auto &p : pedestrians) renderPedestrian(p, depthShader, cubeVAO, cylinderVAO);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, WIDTH, HEIGHT);

                // Bind depth map to a texture unit for use in lighting pass
                buildingShader.Use();
                buildingShader.SetInt("spotShadowMap", 5);
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, depthMap);
                buildingShader.SetMat4("spotLightSpace", lightSpaceMatrix);
            }
        }
        else {
            // Point light
            buildingShader.SetVec3("lightPos", glm::vec3(50.0f, 80.0f, 50.0f));
            buildingShader.SetVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.8f)); // Warm sunlight
            buildingShader.SetVec3("ambientColor", glm::vec3(0.2f, 0.2f, 0.3f));

            // If using point-light mode, also set zero point lights and spotlights arrays
            buildingShader.SetInt("numPointLights", 0);
            buildingShader.SetInt("numSpotLights", 0);
        }
        buildingShader.SetVec3("viewPos", camera.GetPosition());

        // Render ground (grass/concrete)
        glm::vec3 groundColor = glm::vec3(0.4f, 0.6f, 0.3f);  // Grass green
        if (timeOfDay >= 6.0f && timeOfDay < 10.0f) groundColor = glm::vec3(0.52f, 0.75f, 0.45f); // Brighter in morning

        buildingShader.SetBool("isGround", true);
        buildingShader.SetFloat("groundBumpIntensity", 0.4f);
        buildingShader.SetFloat("time", (float)glfwGetTime());
        buildingShader.SetMat4("projection", projection);
        buildingShader.SetMat4("view", view);

        model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(250.0f, 1.0f, 250.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", groundColor);
        // groundVAO has an element array buffer (EBO) with index count from createGround(); use it for indexed draw
        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, getGroundIndexCount(), GL_UNSIGNED_INT, 0);

        buildingShader.SetBool("isGround", false);
        buildingShader.SetFloat("groundBumpIntensity", 0.0f);

        // === RENDER HIGHWAY SYSTEM ===
        // Main East-West Highway (25m wide total)
        for (int x = -120; x <= 120; x += 5) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, 0.02f, 0.0f));
            model = glm::scale(model, glm::vec3(5.0f, 0.04f, 25.0f));  // 25m wide highway
            glm::vec3 roadColor = glm::vec3(0.25f, 0.25f, 0.25f);
            if (timeOfDay >= 6.0f && timeOfDay < 10.0f) roadColor = glm::vec3(0.38f, 0.38f, 0.38f); // Brighter in morning
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", roadColor);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Sidewalks along East-West highway (both sides)
            glm::vec3 sidewalkColor = glm::vec3(0.65f, 0.65f, 0.66f);
            float sidewalkWidth = 2.5f; // 2.5m sidewalk depth
            float sidewalkOffsetZ = 12.5f + sidewalkWidth / 2.0f + 0.3f; // 12.5 is half of 25m highway
            // Right side
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, 0.03f, -sidewalkOffsetZ));
            model = glm::scale(model, glm::vec3(5.0f, 0.02f, sidewalkWidth));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", sidewalkColor);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            // Left side
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, 0.03f, sidewalkOffsetZ));
            model = glm::scale(model, glm::vec3(5.0f, 0.02f, sidewalkWidth));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", sidewalkColor);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // East-West Highway lane markings (white dashed lines)
        for (int x = -120; x <= 120; x += 8) {  // Dashed line pattern
            // Right lane marking
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, 0.06f, -3.0f));
            model = glm::scale(model, glm::vec3(4.0f, 0.02f, 0.2f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.9f));  // White

            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Left lane marking
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, 0.06f, 3.0f));
            model = glm::scale(model, glm::vec3(4.0f, 0.02f, 0.2f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.9f));

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Highway center divider (solid yellow line)
        for (int x = -120; x <= 120; x += 2) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, 0.07f, 0.0f));
            model = glm::scale(model, glm::vec3(2.0f, 0.02f, 0.3f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.1f));  // Yellow divider

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // North-South Highway (25m wide total)  
        for (int z = -120; z <= 120; z += 5) {
            // Skip intersection area to avoid overlapping
            if (z >= -15 && z <= 15) continue;

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.02f, z));
            model = glm::scale(model, glm::vec3(25.0f, 0.04f, 5.0f));  // 25m wide highway
            glm::vec3 roadColor = glm::vec3(0.25f, 0.25f, 0.25f);
            if (timeOfDay >= 6.0f && timeOfDay < 10.0f) roadColor = glm::vec3(0.38f, 0.38f, 0.38f);
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", roadColor);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Sidewalks along North-South highway (both sides)
            glm::vec3 sidewalkColor = glm::vec3(0.65f, 0.65f, 0.66f);
            float sidewalkWidth = 2.5f;
            float sidewalkOffsetX = 12.5f + sidewalkWidth / 2.0f + 0.3f;
            // Right side (positive X)
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(sidewalkOffsetX, 0.03f, z));
            model = glm::scale(model, glm::vec3(sidewalkWidth, 0.02f, 5.0f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", sidewalkColor);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            // Left side (negative X)
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-sidewalkOffsetX, 0.03f, z));
            model = glm::scale(model, glm::vec3(sidewalkWidth, 0.02f, 5.0f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", sidewalkColor);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // North-South Highway lane markings
        for (int z = -120; z <= 120; z += 8) {
            if (z >= -15 && z <= 15) continue;  // Skip intersection

            // Right lane marking
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(3.0f, 0.06f, z));
            model = glm::scale(model, glm::vec3(0.2f, 0.02f, 4.0f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.9f));

            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Left lane marking
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-3.0f, 0.06f, z));
            model = glm::scale(model, glm::vec3(0.2f, 0.02f, 4.0f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.9f));

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // North-South Highway center divider
        for (int z = -120; z <= 120; z += 2) {
            if (z >= -15 && z <= 15) continue;

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.07f, z));
            model = glm::scale(model, glm::vec3(0.3f, 0.02f, 2.0f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.1f));  // Yellow divider

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Highway intersection (full coverage)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.02f, 0.0f));
        model = glm::scale(model, glm::vec3(25.0f, 0.04f, 25.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.25f, 0.25f, 0.25f));

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // === ADD ZEBRA CROSSWALKS ===
        // Draw a single crosswalk on each main highway (one per road) to avoid clutter.
        buildingShader.SetVec3("objectColor", glm::vec3(0.95f, 0.95f, 0.95f)); // near-white for stripes

        // East-West highway: place one crosswalk at X = 0 (across z ~ 0)
        {
            float centerX = 0.0f;
            float baseY = 0.075f; // slightly above road to avoid Z-fighting
            int stripes = 8;                // number of stripes
            float stripeWidth = 10.0f;      // length along X (keep moderate)
            float stripeDepth = 1.2f;       // thickness along Z (stripe 'height' across road)
            float spacing = 3.2f;           // spacing between stripes, chosen to span ~25m road width
            int half = stripes / 2;
            for (int s = -half; s <= half; ++s) {
                model = glm::mat4(1.0f);
                float zPos = s * spacing;
                model = glm::translate(model, glm::vec3(centerX, baseY, zPos));
                model = glm::scale(model, glm::vec3(stripeWidth, 0.01f, stripeDepth));
                buildingShader.SetMat4("model", model);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

        // === RENDER ROAD INFRASTRUCTURE ===
        renderRoadInfrastructure(buildingShader, cubeVAO, glfwGetTime());

        // === RENDER TREES ===


        // === RENDER REALISTIC MOVING CARS ===
        // Spatial partitioning: build quadtrees for moving cars and pedestrians and query items near camera
        {
            // define bounds slightly larger than world size used elsewhere
            const float worldHalf = 180.0f;
            QuadNode carsRoot(glm::vec2(0.0f, 0.0f), worldHalf);
            for (size_t i = 0; i < cars.size(); ++i) carsRoot.Insert(cars[i].position, static_cast<int>(i));

            QuadNode pedsRoot(glm::vec2(0.0f, 0.0f), worldHalf);
            for (size_t i = 0; i < pedestrians.size(); ++i) pedsRoot.Insert(pedestrians[i].position, static_cast<int>(i));

            // Query radius based on camera distance (LOD). Use a reasonable view radius.
            float viewRadius = 60.0f; // meters
            glm::vec2 cam2d = glm::vec2(camera.GetPosition().x, camera.GetPosition().z);
            std::vector<int> nearbyCarIds; nearbyCarIds.reserve(64);
            carsRoot.QueryRange(cam2d, viewRadius, nearbyCarIds);
            for (int id : nearbyCarIds) {
                if (id >= 0 && id < (int)cars.size()) renderRealisticCar(cars[id], buildingShader, cubeVAO, cylinderVAO);
            }

            std::vector<int> nearbyPedIds; nearbyPedIds.reserve(128);
            pedsRoot.QueryRange(cam2d, viewRadius, nearbyPedIds);
            for (int id : nearbyPedIds) {
                if (id >= 0 && id < (int)pedestrians.size()) renderPedestrian(pedestrians[id], buildingShader, cubeVAO, cylinderVAO);
            }
        }

        // Render shopping mall complex (shops + parking lot + parked cars)
        renderShoppingMallComplex(buildingShader, cubeVAO, cylinderVAO);

        // Realistic building colors
        glm::vec3 houseColors[] = {
            glm::vec3(0.9f, 0.9f, 0.85f),  // Off-white/cream
            glm::vec3(0.8f, 0.7f, 0.6f),   // Beige/tan
            glm::vec3(0.7f, 0.6f, 0.5f),   // Light brown
            glm::vec3(0.85f, 0.8f, 0.75f), // Light gray
            glm::vec3(0.9f, 0.85f, 0.7f),  // Warm white
            glm::vec3(0.75f, 0.7f, 0.65f), // Light taupe
            glm::vec3(0.6f, 0.5f, 0.4f),   // Medium brown
            glm::vec3(0.8f, 0.75f, 0.7f)   // Pink-beige
        };

        // Roof colors (darker, more realistic)
        glm::vec3 roofColors[] = {
            glm::vec3(0.4f, 0.2f, 0.2f),   // Dark red (clay tiles)
            glm::vec3(0.3f, 0.3f, 0.3f),   // Dark gray (slate)
            glm::vec3(0.2f, 0.15f, 0.1f),  // Dark brown (wooden)
            glm::vec3(0.25f, 0.25f, 0.25f), // Charcoal gray
            glm::vec3(0.35f, 0.25f, 0.15f), // Burnt orange
            glm::vec3(0.5f, 0.3f, 0.2f),   // Terracotta
            glm::vec3(0.2f, 0.2f, 0.3f),   // Blue-gray slate
            glm::vec3(0.45f, 0.35f, 0.25f) // Reddish brown
        };

        // Buildings already generated at startup; draw them now
        // Seed random for consistent building generation
        srand(12345);

        // Collect placed building boxes so trees can avoid them
        std::vector<glm::vec4> buildingBoxes; // x, z, halfWidth, halfDepth

        // === FIRST: ADD ICONIC TOWERS ===

        if (gWireframeAll) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        // KL Tower (Kuala Lumpur Tower) - 421m tall
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(25.0f, 210.5f, 25.0f));
        model = glm::scale(model, glm::vec3(3.0f, 421.0f, 3.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.8f, 0.8f, 0.9f));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // KL Tower antenna/spire
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(25.0f, 421.0f + 15.0f, 25.0f));
        model = glm::scale(model, glm::vec3(0.5f, 30.0f, 0.5f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.1f, 0.1f));
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // KL Tower observation deck
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(25.0f, 350.0f, 25.0f));
        model = glm::scale(model, glm::vec3(8.0f, 10.0f, 8.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.7f, 0.7f, 0.8f));
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // KL Tower top glow (night only)
        if (skyIntensity < 0.25f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            float glowAlpha = glm::clamp(1.0f - skyIntensity * 4.0f, 0.0f, 1.0f);
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(25.0f, 421.0f + 30.0f, 25.0f));
            model = glm::scale(model, glm::vec3(16.0f, 12.0f, 16.0f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(1.0f, 0.98f, 0.8f) * glowAlpha);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glDisable(GL_BLEND);
        }

        // Eiffel Tower (Paris Tower) - 330m tall
        // Tower base (wider at bottom)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-30.0f, 50.0f, -30.0f));
        model = glm::scale(model, glm::vec3(12.0f, 100.0f, 12.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.6f, 0.5f, 0.4f));  // Iron color

        glDrawArrays(GL_TRIANGLES, 0, 36);

        buildingBoxes.emplace_back(glm::vec4(-30.0f, -30.0f, 12.0f * 0.5f, 100.0f * 0.5f));

        // Tower middle section
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-30.0f, 150.0f, -30.0f));
        model = glm::scale(model, glm::vec3(8.0f, 100.0f, 8.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.6f, 0.5f, 0.4f));

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Tower top section
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-30.0f, 250.0f, -30.0f));
        model = glm::scale(model, glm::vec3(4.0f, 100.0f, 4.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.6f, 0.5f, 0.4f));

        glDrawArrays(GL_TRIANGLES, 0, 36);

        buildingBoxes.emplace_back(glm::vec4(-30.0f, -30.0f, 4.0f * 0.5f, 100.0f * 0.5f));

        // Eiffel Tower antenna
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-30.0f, 315.0f, -30.0f));
        model = glm::scale(model, glm::vec3(0.8f, 30.0f, 0.8f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.5f, 0.4f, 0.3f));

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Eiffel Tower top glow (night only)
        if (skyIntensity < 0.25f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            float glowAlpha = glm::clamp(1.0f - skyIntensity * 4.0f, 0.0f, 1.0f);
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-30.0f, 315.0f + 15.0f, -30.0f));
            model = glm::scale(model, glm::vec3(12.0f, 10.0f, 12.0f));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(1.0f, 0.98f, 0.8f) * glowAlpha);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glDisable(GL_BLEND);
        }

        // Draw deterministic city generated at startup
        drawCity(buildings, smallTrees, buildingShader, cubeVAO, roofVAO, cylinderVAO);
        // Call residential decorative lights after buildings are placed
        renderResidentialLights(buildingShader, cubeVAO, cylinderVAO, buildingBoxes, (float)glfwGetTime());

        if (gWireframeAll) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Render external model visiongt if loaded
        if (visionLoaded) {
            glm::mat4 vm = glm::mat4(1.0f);
            vm = glm::translate(vm, visionPosition);
            vm = glm::rotate(vm, visionYaw, glm::vec3(0.0f, 1.0f, 0.0f));
            vm = glm::scale(vm, glm::vec3(visionScale, visionScale, visionScale));
            // ensure model uses current building shader
            visionModel.Draw(buildingShader, vm);
        }

        // --- SECOND PASS: additive glow pass for point lights and spotlights (multi-pass lighting enhancement)
        if (!pointPositions.empty() || !spotPositions.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glowShader.Use();
            glowShader.SetMat4("projection", projection);
            glowShader.SetMat4("view", view);

            // By default hide the small emissive cube markers (set to true for debugging)
            bool showLightMarkers = false;

            if (showLightMarkers) {
                // Render small additive cubes at point light positions
                for (size_t i = 0; i < pointPositions.size(); ++i) {
                    glm::vec3 p = pointPositions[i];
                    glm::mat4 m = glm::mat4(1.0f);
                    float scale = 0.5f + pointIntensities[i] * 0.25f;
                    m = glm::translate(m, p);
                    m = glm::scale(m, glm::vec3(scale));
                    glowShader.SetMat4("model", m);
                    glm::vec3 col = pointColors[i] * glm::clamp(pointIntensities[i], 0.3f, 6.0f);
                    glowShader.SetVec3("glowColor", col);
                    glowShader.SetFloat("glowAlpha", 0.6f);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }

                // Render small additive cubes at spotlight positions
                for (size_t i = 0; i < spotPositions.size(); ++i) {
                    glm::vec3 p = spotPositions[i];
                    glm::mat4 m = glm::mat4(1.0f);
                    float scale = 0.8f + spotIntensities[i] * 0.25f;
                    m = glm::translate(m, p);
                    m = glm::scale(m, glm::vec3(scale));
                    glowShader.SetMat4("model", m);
                    glm::vec3 col = spotColors[i] * glm::clamp(spotIntensities[i], 0.3f, 8.0f);
                    glowShader.SetVec3("glowColor", col);
                    glowShader.SetFloat("glowAlpha", 0.7f);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            }

            glDisable(GL_BLEND);
        }

        // --- VOLMETRIC SPOTLIGHT BEAMS FOR TALL BUILDINGS (KL, Eiffel) ---
        if (!spotPositions.empty()) {
            // Render additive volumetric beams aligned with spot directions
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            // Do not write to depth buffer so beams blend over the scene
            glDepthMask(GL_FALSE);
            spotlightShader.Use();
            spotlightShader.SetMat4("projection", projection);
            spotlightShader.SetMat4("view", view);

            for (size_t i = 0; i < spotPositions.size(); ++i) {
                glm::vec3 apex = spotPositions[i];
                glm::vec3 dir = glm::normalize(spotDirections[i]);
                glm::vec3 color = spotColors[i];
                float intensity = spotIntensities[i] * 0.12f; // tune down for visual
                // Beam parameters
                float beamLength = 120.0f; // meters - how far beam reaches
                float beamRadius = 30.0f;  // meters at base

                // Build model: cylinder of height=1 (createCylinder uses h=1) scaled so that its top aligns with apex
                // Cylinder local up is +Y; we want top at apex, so translate center to apex - dir*(0.5*beamLength)
                glm::vec3 center = apex - dir * (0.5f * beamLength);

                // rotation from +Y to dir
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (fabs(glm::dot(up, dir)) > 0.99f) up = glm::vec3(1.0f, 0.0f, 0.0f);
                glm::mat4 rot = glm::mat4(1.0f);
                float d = glm::dot(up, dir);
                if (d > 0.9999f) {
                    rot = glm::mat4(1.0f);
                } else if (d < -0.9999f) {
                    // 180 degree rotation around X axis (or any orthogonal axis)
                    glm::quat q = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
                    rot = glm::mat4_cast(q);
                } else {
                    glm::vec3 axis = glm::normalize(glm::cross(up, dir));
                    float angle = acos(glm::clamp(d, -1.0f, 1.0f));
                    glm::quat q = glm::angleAxis(angle, axis);
                    rot = glm::mat4_cast(q);
                }

                // scale: cylinder initial radius=0.5, height=1.0 (see createCylinder), so scale x/z by (beamRadius / 0.5) and y by beamLength
                glm::mat4 modelSpot = glm::mat4(1.0f);
                modelSpot = glm::translate(modelSpot, center);
                modelSpot *= rot;
                modelSpot = glm::scale(modelSpot, glm::vec3(beamRadius / 0.5f, beamLength, beamRadius / 0.5f));

                spotlightShader.SetMat4("model", modelSpot);
                spotlightShader.SetVec3("beamApex", apex);
                spotlightShader.SetVec3("beamDir", dir);
                spotlightShader.SetFloat("beamLength", beamLength);
                spotlightShader.SetFloat("beamRadius", beamRadius);
                spotlightShader.SetVec3("beamColor", color);
                spotlightShader.SetFloat("beamIntensity", intensity);

                glBindVertexArray(cylinderVAO);
                // cylinder uses indexed draw with ~segments*6 indices
                // We know createCylinder used EBO and returned VAO; use glDrawElements with an estimated count
                // Safer: draw arrays fallback: the cylinder uses index buffer; but VAO/EBO set, so call glDrawElements with count
                // From createCylinder implementation, index count = segments * 6 (segments = 8) => 48
                glDrawElements(GL_TRIANGLES, 8 * 6, GL_UNSIGNED_INT, 0);
            }

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // draw skybox
        // Before skybox, render sea + beach so reflections include scene
        {
            // Sea/beach parameters
            float outerSeaWidth = 160.0f; // total outer footprint including beach
            float outerSeaDepth = 120.0f;
            float beachBand = 20.0f; // single horizontal beach band that separates water from roads/buildings
            // water is confined to inner rect so it won't reach roads/buildings
            float waterWidth = glm::max(2.0f, outerSeaWidth - 2.0f * beachBand);
            float waterDepth = glm::max(2.0f, outerSeaDepth - 2.0f * beachBand);
            
            // place beach slightly above water so water never floods surrounding geometry
            float sandHeight = 0.02f; // thin band
            float waterY = poolPos.y - 0.06f; // lower water a bit (avoid flooding building bases at y=0)

            // Compute sun lighting for water/beach once
            glm::vec3 sunDir = glm::normalize(glm::vec3(cos((timeOfDay/24.0f)*2.0f*M_PI), sin((timeOfDay/24.0f)*2.0f*M_PI), 0.0f));
            float sunIntensity = (timeOfDay >= 6.0f && timeOfDay <= 18.0f) ? 1.0f : 0.2f;
            glm::vec3 sunColor = glm::vec3(1.0f, 0.95f, 0.9f) * sunIntensity;

            // Draw beach bands using buildingShader
            buildingShader.Use();
            buildingShader.SetMat4("projection", projection);
            buildingShader.SetMat4("view", view);
            buildingShader.SetFloat("time", (float)glfwGetTime());
            buildingShader.SetVec3("viewPos", camera.GetPosition());
            buildingShader.SetVec3("baseColor", glm::vec3(0.02f, 0.18f, 0.28f));

            auto drawBeachX = [&](float centerZ, float sizeX, float sizeZ){
                glm::vec3 beachColor = glm::vec3(0.94f, 0.85f, 0.62f); // sand color
                glm::mat4 bm = glm::mat4(1.0f);
                bm = glm::translate(bm, glm::vec3(poolPos.x, waterY + sandHeight * 0.5f, poolPos.z + centerZ));
                bm = glm::scale(bm, glm::vec3(sizeX, sandHeight, sizeZ));
                buildingShader.SetMat4("model", bm);
                buildingShader.SetVec3("objectColor", beachColor);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            };
            auto drawBeachZ = [&](float centerX, float sizeX, float sizeZ){
                glm::vec3 beachColor = glm::vec3(0.94f, 0.85f, 0.62f);
                glm::mat4 bm = glm::mat4(1.0f);
                bm = glm::translate(bm, glm::vec3(poolPos.x + centerX, waterY + sandHeight * 0.5f, poolPos.z));
                bm = glm::scale(bm, glm::vec3(sizeX, sandHeight, sizeZ));
                buildingShader.SetMat4("model", bm);
                buildingShader.SetVec3("objectColor", beachColor);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            };

            float outerX = outerSeaWidth + 2.0f * 0.0f; // outer extents
            float outerZ = outerSeaDepth + 2.0f * 0.0f;

            // North / South beach bands
            drawBeachX((waterDepth * 0.6f) + (beachBand * 0.7f), outerX, beachBand);
            drawBeachX(-(waterDepth * 0.5f) - (beachBand * 0.5f), outerX, beachBand);
            // East / West beach bands
            drawBeachZ((waterWidth * 0.5f) + (beachBand * 0.5f), beachBand, outerZ);
            drawBeachZ(-(waterWidth * 0.5f) - (beachBand * 0.5f), beachBand, outerZ);

            // Render water using water shader and the ground mesh (indexed)
            waterShader.Use();
            waterShader.SetMat4("projection", projection);
            waterShader.SetMat4("view", view);
            waterShader.SetFloat("time", (float)glfwGetTime());
            waterShader.SetVec3("viewPos", camera.GetPosition());
            waterShader.SetVec3("baseColor", glm::vec3(0.02f, 0.18f, 0.28f));
            waterShader.SetVec3("sunDir", sunDir);
            waterShader.SetVec3("sunColor", sunColor);

            glm::mat4 seaModel = glm::mat4(1.0f);
            seaModel = glm::translate(seaModel, glm::vec3(poolPos.x, waterY, poolPos.z));
            seaModel = glm::scale(seaModel, glm::vec3(waterWidth, 1.0f, waterDepth));
            waterShader.SetMat4("model", seaModel);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
            waterShader.SetInt("skybox", 0);

            static GLuint dudvTex = 0;
            if (dudvTex == 0) {
                std::string dudvPath = "Textures/water_dudv.png";
                int w,h,nc;
                unsigned char* data = stbi_load(dudvPath.c_str(), &w, &h, &nc, 0);
                if (data) {
                    glGenTextures(1, &dudvTex);
                    glBindTexture(GL_TEXTURE_2D, dudvTex);
                    GLenum fmt = (nc==4)?GL_RGBA:GL_RGB;
                    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    stbi_image_free(data);
                }
            }
            if (dudvTex) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, dudvTex);
                waterShader.SetInt("dudvMap", 1);
            }

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindVertexArray(groundVAO);
            glDrawElements(GL_TRIANGLES, getGroundIndexCount(), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glDisable(GL_BLEND);
        }

        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxShader);

        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Pass time of day and sky intensity to skybox shader
        glUniform1f(glGetUniformLocation(skyboxShader, "timeOfDay"), timeOfDay);
        glUniform1f(glGetUniformLocation(skyboxShader, "skyIntensity"), skyIntensity);

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // Render GUI last
        SimpleGUI::Instance().Draw(deltaTime);
        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteVertexArrays(1, &roofVAO);
    glDeleteVertexArrays(1, &cylinderVAO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(skyboxShader);

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    if (gUseDeferred) {
        gDeferred.Resize(width, height);
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void processInput(GLFWwindow* window)
{
    static Shader* currentShader = nullptr;
    static bool key1Pressed=false, key2Pressed=false, key3Pressed=false;
    // Wireframe toggles
    if(glfwGetKey(window, GLFW_KEY_1)==GLFW_PRESS && !key1Pressed){ key1Pressed=true; gWireframeAll = !gWireframeAll; std::cout<<"[Wireframe-All] "<<(gWireframeAll?"ON":"OFF")<<std::endl; }
    if(glfwGetKey(window, GLFW_KEY_1)==GLFW_RELEASE) key1Pressed=false;
    if(glfwGetKey(window, GLFW_KEY_2)==GLFW_PRESS && !key2Pressed){ key2Pressed=true; gWireframeRoofs = !gWireframeRoofs; std::cout<<"[Wireframe-Roofs] "<<(gWireframeRoofs?"ON":"OFF")<<std::endl; }
    if(glfwGetKey(window, GLFW_KEY_2)==GLFW_RELEASE) key2Pressed=false;
    if(glfwGetKey(window, GLFW_KEY_3)==GLFW_PRESS && !key3Pressed){ key3Pressed=true; gWireframeBlock = !gWireframeBlock; std::cout<<"[Wireframe-Block Node] "<<(gWireframeBlock?"ON":"OFF")<<std::endl; }
    if(glfwGetKey(window, GLFW_KEY_3)==GLFW_RELEASE) key3Pressed=false;

    // Toggle deferred rendering (F6)
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS && !gDeferredKeyPressed) {
        gDeferredKeyPressed = true;
        gUseDeferred = !gUseDeferred;
        if (gUseDeferred) {
            if (!gDeferred.Initialize(1920,1080)) {
                std::cout << "Deferred renderer init failed, reverting to forward." << std::endl;
                gUseDeferred = false;
            } else {
                std::cout << "Deferred Rendering: ON" << std::endl;
            }
        } else {
            std::cout << "Deferred Rendering: OFF" << std::endl;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_RELEASE) gDeferredKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Camera movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    // Space to jump in FIRST_PERSON
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (camera.Mode == Camera::FIRST_PERSON) camera.Jump();
    }

    // Camera mode switching
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cameraKeyPressed) {
        cameraKeyPressed = true;
        static int currentMode = 0;
        currentMode = (currentMode + 1) % 3;  // 3 modes: FREE_FLY, FIRST_PERSON, ORBITAL
        switch (currentMode) {
        case 0: camera.SetCameraMode(Camera::FREE_FLY); std::cout << "Camera Mode: FREE FLY" << std::endl; break;
        case 1: camera.SetCameraMode(Camera::FIRST_PERSON); std::cout << "Camera Mode: FIRST PERSON" << std::endl; break;
        case 2: camera.SetCameraMode(Camera::ORBITAL); camera.SetOrbitTarget(glm::vec3(0.0f, 8.0f, 0.0f), 40.0f); std::cout << "Camera Mode: ORBITAL" << std::endl; break;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) cameraKeyPressed = false;

    // Reset camera
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camera.ResetToDefault();
        std::cout << "Camera Reset" << std::endl;
    }

    // Hot reload shaders (F5)
    static bool f5KeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS && !f5KeyPressed) { f5KeyPressed = true; std::cout << "Reloading shaders from files..." << std::endl; }
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_RELEASE) f5KeyPressed = false;

    // Speed adjustment
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera.MovementSpeed = 25.0f; else camera.MovementSpeed = 12.0f;

    // Light mode
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS && !lightKeyPressed) { lightKeyPressed = true; useDirectionalLight = !useDirectionalLight; std::cout << "Light mode: " << (useDirectionalLight?"Directional":"Point") << std::endl; }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_RELEASE) lightKeyPressed = false;

    // MSAA switch (M cycles samples)
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !msaaKeyPressed) {
        msaaKeyPressed = true;
        if (MSAA > 0) { MSAA = 0; glDisable(GL_MULTISAMPLE); if(gUseDeferred) gDeferred.SetMSAASamples(0); std::cout << "MSAA: OFF" << std::endl; }
        else { static int cycle = 0; int samplesCycle[3]={4,8,2}; int chosen = samplesCycle[cycle]; cycle=(cycle+1)%3; MSAA=chosen; glEnable(GL_MULTISAMPLE); if(gUseDeferred) gDeferred.SetMSAASamples(chosen); std::cout << "MSAA: ON ("<<chosen<<"x)" << std::endl; }
    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE) msaaKeyPressed = false;

    // Bloom controls (deferred only) -- B toggles, N increases intensity
    static bool bloomTogglePressed = false;
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        if(!bloomTogglePressed){ bloomTogglePressed=true; if(gUseDeferred){ gDeferred.SetBloomEnabled(!gDeferred.IsBloomEnabled()); std::cout<<"Bloom: "<<(gDeferred.IsBloomEnabled()?"ON":"OFF")<<std::endl; }}
    } else { bloomTogglePressed=false; }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) { if(gUseDeferred){ float bi=gDeferred.GetBloomIntensity(); gDeferred.SetBloomIntensity(std::min(5.0f, bi + 0.5f*deltaTime)); } }

    // HDR exposure controls J/K
    static float exposureChangeRate = 0.5f;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS && gUseDeferred) { float exposure = gDeferred.GetExposure(); gDeferred.SetExposure(exposure + exposureChangeRate * deltaTime); }
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS && gUseDeferred) { float exposure = gDeferred.GetExposure(); gDeferred.SetExposure(std::max(0.1f, exposure - exposureChangeRate * deltaTime)); }

    // Toggle GUI visibility F1
    static bool f1Pressed=false; if(glfwGetKey(window,GLFW_KEY_F1)==GLFW_PRESS){ if(!f1Pressed){ f1Pressed=true; SimpleGUI::Instance().ToggleVisible(); }} else if(glfwGetKey(window,GLFW_KEY_F1)==GLFW_RELEASE){ f1Pressed=false; }
}

GLuint createCube()
{
    static const float srcVertices[] = {
        -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
         0.5f,-0.5f,-0.5f, 0,0,-1, 1,0,
         0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
         0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
        -0.5f, 0.5f,-0.5f, 0,0,-1, 0,1,
        -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
        -0.5f,-0.5f, 0.5f, 0,0, 1, 0,0,
         0.5f,-0.5f, 0.5f, 0,0, 1, 1,0,
         0.5f, 0.5f, 0.5f, 0,0, 1, 1,1,
         0.5f, 0.5f, 0.5f, 0,0, 1, 1,1,
        -0.5f, 0.5f, 0.5f, 0,0, 1, 0,1,
        -0.5f,-0.5f, 0.5f, 0,0, 1, 0,0,
        -0.5f, 0.5f, 0.5f,-1,0,0, 1,0,
        -0.5f, 0.5f,-0.5f,-1,0,0, 1,1,
        -0.5f,-0.5f,-0.5f,-1,0,0, 0,1,
        -0.5f,-0.5f,-0.5f,-1,0,0, 0,1,
        -0.5f,-0.5f, 0.5f,-1,0,0, 0,0,
        -0.5f, 0.5f, 0.5f,-1,0,0, 1,0,
         0.5f, 0.5f, 0.5f, 1,0,0, 1,0,
         0.5f, 0.5f,-0.5f, 1,0,0, 1,1,
         0.5f,-0.5f,-0.5f, 1,0,0, 0,1,
         0.5f,-0.5f,-0.5f, 1,0,0, 0,1,
         0.5f,-0.5f, 0.5f, 1,0,0, 0,0,
         0.5f, 0.5f, 0.5f, 1,0,0, 1,0,
        -0.5f,-0.5f,-0.5f, 0,-1,0, 0,1,
         0.5f,-0.5f,-0.5f, 0,-1,0, 1,1,
         0.5f,-0.5f, 0.5f, 0,-1,0, 1,0,
         0.5f,-0.5f, 0.5f, 0,-1,0, 1,0,
        -0.5f,-0.5f, 0.5f, 0,-1,0, 0,0,
        -0.5f,-0.5f,-0.5f, 0,-1,0, 0,1,
        -0.5f, 0.5f,-0.5f, 0, 1,0, 0,1,
         0.5f, 0.5f,-0.5f, 0, 1,0, 1,1,
         0.5f, 0.5f, 0.5f, 0, 1,0, 1,0,
         0.5f, 0.5f, 0.5f, 0, 1,0, 1,0,
        -0.5f, 0.5f, 0.5f, 0, 1,0, 0,0,
        -0.5f, 0.5f,-0.5f, 0, 1,0, 0,1
    };
    GLuint VAO,VBO; glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO); glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER,VBO); glBufferData(GL_ARRAY_BUFFER,sizeof(srcVertices),srcVertices,GL_STATIC_DRAW); GLsizei stride = 8*sizeof(float); glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0); glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float))); glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float))); glBindVertexArray(0); return VAO;
}