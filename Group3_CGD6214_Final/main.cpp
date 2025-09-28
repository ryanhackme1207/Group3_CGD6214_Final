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
#include "Camera.h"
#include "Shader.h"
#include "Mesh.h"
#include "Model.h"
#include "SceneNode.h"
#include "SceneGraph.h"
#include "LODManager.h"
#include "SpatialPartition.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//global state
bool useDirectionalLight = true;
bool lightKeyPressed = false;

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

// Car structure for moving vehicles
struct Car {
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 color;
    float speed;
    float width = 0.0f;
    float height = 0.0f;
    float length = 0.0f;
    int lane;  // 0 = right lane, 1 = left lane
    int roadType; // 0 = main road X, 1 = main road Z, 2 = highway X, 3 = highway Z
    int carType; // 0 = sedan, 1 = SUV, 2 = truck, 3 = hatchback

    Car(glm::vec3 pos, glm::vec3 dir, glm::vec3 col, float spd, int ln, int rt, int ct)
        : position(pos), direction(dir), color(col), speed(spd), lane(ln), roadType(rt), carType(ct), width(0.0f), height(0.0f), length(0.0f) {

        // Different dimensions based on car type
        switch (carType) {
        case 0: // Sedan
            width = 1.8f; height = 1.4f; length = 4.5f;
            break;
        case 1: // SUV
            width = 2.0f; height = 1.8f; length = 4.8f;
            break;
        case 2: // Truck
            width = 2.2f; height = 2.0f; length = 5.5f;
            break;
        case 3: // Hatchback
            width = 1.7f; height = 1.5f; length = 3.8f;
            break;
        }
    }
};

std::vector<Car> cars;
std::vector<Car> parkedCars; // parked cars in parking lots
float carSpawnTimer = 0.0f;

// New: human prototypes
void spawnPedestrians(int count);
void updatePedestrians(float deltaTime);
void renderPedestrian(const struct Pedestrian& p, Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);

// Structure for human characters
struct Pedestrian {
    glm::vec3 position;
    glm::vec3 direction;
    float speed;
    float walkTimer;
    float walkDuration;
    float bodyHeight;
    glm::vec3 color;
    float walkCyclePhase; // NEW: phase for limb animation
    Pedestrian(glm::vec3 pos, glm::vec3 dir, float spd, float dur, glm::vec3 col)
        : position(pos), direction(dir), speed(spd), walkTimer(0.0f), walkDuration(dur), bodyHeight(1.75f), color(col), walkCyclePhase(0.0f) {}
};

std::vector<Pedestrian> pedestrians;

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
GLuint createCube();
GLuint createGround();
GLuint createTriangularRoof();
GLuint createCylinder();
void updateCars(float deltaTime);
void spawnCar();
void renderRoadInfrastructure(Shader& shader, GLuint cubeVAO, float currentTime);
void renderRealisticCar(const Car& car, Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);
void renderTrees(Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);
void renderShoppingMallComplex(Shader& shader, GLuint cubeVAO, GLuint cylinderVAO); // new prototype

// Pedestrian prototypes
void spawnPedestrians(int count);
void updatePedestrians(float deltaTime);
void renderPedestrian(const struct Pedestrian& p, Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);

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

int main()
{
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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

    // Create geometry
    GLuint cubeVAO = createCube();
    GLuint groundVAO = createGround();
    GLuint roofVAO = createTriangularRoof();
    GLuint cylinderVAO = createCylinder();

    // Populate parked cars for the shopping mall parking lot
    // Mall center at (60, 0, 60)
    srand(424242); // fixed seed for consistent parked car placement
    glm::vec3 mallCenter = glm::vec3(60.0f, 0.0f, 60.0f);
    int rows = 4;
    int cols = 10;
    float spacingX = 4.5f; // space between parking spaces
    float spacingZ = 6.0f; // aisle spacing
    float startX = mallCenter.x - (cols/2.0f - 0.5f) * spacingX;
    float startZ = mallCenter.z - (rows/2.0f - 0.5f) * spacingZ - 30.0f; // place parking in front of mall
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float px = startX + c * spacingX + ((rand()%100)/100.0f - 0.5f) * 0.3f;
            float pz = startZ + r * spacingZ + ((rand()%100)/100.0f - 0.5f) * 0.3f;
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

    // Render loop
    glm::mat4 model;
    while (!glfwWindowShouldClose(window))
    {
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

        // Update cars
        carSpawnTimer += deltaTime;
        if (carSpawnTimer > 1.5f) {  // Spawn a car every 1.5 seconds for more traffic
            spawnCar();
            carSpawnTimer = 0.0f;
        }
        updateCars(deltaTime);

        // Update pedestrians
        updatePedestrians(deltaTime);

        // Update camera
        camera.UpdateSmoothMovement(deltaTime);
        camera.UpdateOrbitalCamera(deltaTime);
        camera.UpdateTransition(deltaTime);

        // Background color adjust
        float skyIntensity;
        if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) {
            float dayProgress = (timeOfDay - 6.0f) / 12.0f;
            skyIntensity = 0.6f + 0.2f * sin(dayProgress * M_PI);
            glClearColor(0.6f * skyIntensity, 0.8f * skyIntensity, 1.0f * skyIntensity, 1.0f); // Sky blue background
        }
        else {
            skyIntensity = 0.1f;
            // Night: dark blue/black sky
            glClearColor(0.05f, 0.07f, 0.13f, 1.0f);
        }

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        buildingShader.Use();

        buildingShader.SetFloat("bumpIntensity", 0.3f);
        buildingShader.SetFloat("time", (float)glfwGetTime());

        // View/projection transformations using camera
        glm::mat4 projection = camera.GetProjectionMatrix((float)WIDTH / (float)HEIGHT);
        glm::mat4 view = camera.GetViewMatrix();
        buildingShader.SetMat4("projection", projection);
        buildingShader.SetMat4("view", view);

        // Set lighting uniforms (sun-like lighting)
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

            std::vector<glm::vec3> pointPositions;
            std::vector<glm::vec3> pointColors;
            std::vector<float> pointIntensities;

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
            std::vector<glm::vec3> spotPositions;
            std::vector<glm::vec3> spotDirections;
            std::vector<glm::vec3> spotColors;
            std::vector<float> spotIntensities;
            std::vector<float> spotInner; 
            std::vector<float> spotOuter;

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
        
        model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(250.0f, 1.0f, 250.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", groundColor);
        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

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
            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.1f));

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Highway intersection (full coverage)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.02f, 0.0f));
        model = glm::scale(model, glm::vec3(25.0f, 0.04f, 25.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.25f, 0.25f, 0.25f));

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // === RENDER ROAD INFRASTRUCTURE ===
        renderRoadInfrastructure(buildingShader, cubeVAO, glfwGetTime());

        // === RENDER TREES ===
        renderTrees(buildingShader, cubeVAO, cylinderVAO);

        // === RENDER REALISTIC MOVING CARS ===
        for (const auto& car : cars) {
            renderRealisticCar(car, buildingShader, cubeVAO, cylinderVAO);
        }

        // Render pedestrians
        updatePedestrians(deltaTime);
        for (const auto& p : pedestrians) {
            renderPedestrian(p, buildingShader, cubeVAO, cylinderVAO);
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

        // Seed random for consistent building generation
        srand(12345);

        // === FIRST: ADD ICONIC TOWERS ===

        // KL Tower (Kuala Lumpur Tower) - 421m tall
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(25.0f, 210.5f, 25.0f));  // Half of 421m height
        model = glm::scale(model, glm::vec3(3.0f, 421.0f, 3.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.8f, 0.8f, 0.9f));  // Light metallic

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // KL Tower antenna/spire
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(25.0f, 421.0f + 15.0f, 25.0f));
        model = glm::scale(model, glm::vec3(0.5f, 30.0f, 0.5f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.1f, 0.1f));  // Red antenna

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

        // Create realistic residential neighborhoods
        for (int x = -80; x <= 80; x += 20)  // Wider spacing for realistic lots
        {
            for (int z = -80; z <= 80; z += 20)
            {
                // Create main roads (skip buildings on roads)
                if ((x >= -10 && x <= 10) || (z >= -10 && z <= 10)) {
                    // Render road surface
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(x, 0.01f, z));
                    model = glm::scale(model, glm::vec3(18.0f, 0.02f, 18.0f));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", glm::vec3(0.3f, 0.3f, 0.3f));  // Asphalt

                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    continue;
                }

                // Create secondary streets
                if (x % 40 == 0 || z % 40 == 0) {
                    // Render smaller road
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(x, 0.01f, z));
                    model = glm::scale(model, glm::vec3(15.0f, 0.02f, 15.0f));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", glm::vec3(0.35f, 0.35f, 0.35f));  // Lighter asphalt

                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    continue;
                }

                // Determine building type based on position and random factors
                int buildingType = rand() % 100;
                int colorIndex = rand() % 8;
                int roofColorIndex = rand() % 8;

                float width, depth, height;
                bool hasTriangularRoof = false;
                bool isSemiDetached = false;

                if (buildingType < 40) {
                    // Single family house (40% chance)
                    width = 8.0f + (rand() % 3) * 2.0f;     // 8-12m wide
                    depth = 10.0f + (rand() % 3) * 2.0f;    // 10-14m deep
                    height = 6.0f + (rand() % 2) * 2.0f;    // 6-8m tall (2 stories)
                    hasTriangularRoof = true;
                }
                else if (buildingType < 65) {
                    // Semi-detached house (25% chance)
                    width = 12.0f + (rand() % 2) * 2.0f;    // 12-14m wide
                    depth = 8.0f + (rand() % 2) * 2.0f;     // 8-10m deep
                    height = 7.0f + (rand() % 2) * 1.0f;    // 7-8m tall
                    hasTriangularRoof = true;
                    isSemiDetached = true;
                }
                else if (buildingType < 85) {
                    // Townhouse (20% chance)
                    width = 6.0f + (rand() % 2) * 1.0f;     // 6-7m wide
                    depth = 12.0f + (rand() % 2) * 2.0f;    // 12-14m deep
                    height = 8.0f + (rand() % 2) * 2.0f;    // 8-10m tall (2-3 stories)
                    hasTriangularRoof = (rand() % 2) == 0;  // 50% chance of triangular roof
                }
                else {
                    // Small apartment building (15% chance)
                    width = 15.0f + (rand() % 3) * 3.0f;    // 15-21m wide
                    depth = 12.0f + (rand() % 2) * 3.0f;    // 12-15m deep
                    height = 12.0f + (rand() % 3) * 3.0f;   // 12-18m tall (3-4 stories)
                    hasTriangularRoof = false;  // Flat roof for apartments
                }

                // Add some random variation in position for more natural look
                float xOffset = ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
                float zOffset = ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
                float actualX = x + xOffset;
                float actualZ = z + zOffset;

                // Clamp residential footprints so they fit within a lot and don't overlap nearby infrastructure
                const float residentialMaxW = 12.0f;
                const float residentialMaxD = 14.0f;
                width = glm::min(width, residentialMaxW);
                depth = glm::min(depth, residentialMaxD);

                // Ensure building remains within lot boundaries (keep some padding from road/lights)
                float halfW = width * 0.5f;
                float halfD = depth * 0.5f;
                float lotHalfRes = 10.0f; // half spacing between grid centers
                float paddingRes = 2.5f; // slightly larger padding so buildings don't touch
                float limitRes = lotHalfRes - paddingRes;

                // Try multiple random offsets to avoid overlap/sticking to neighbors
                bool placed = false;
                const int maxAttempts = 6;
                for (int attempt = 0; attempt < maxAttempts && !placed; ++attempt) {
                    float tryX = x + (((rand() % 100) / 100.0f - 0.5f) * limitRes * 0.6f);
                    float tryZ = z + (((rand() % 100) / 100.0f - 0.5f) * limitRes * 0.6f);

                    // Ensure within lot bounds
                    tryX = glm::clamp(tryX, x - limitRes + halfW, x + limitRes - halfW);
                    tryZ = glm::clamp(tryZ, z - limitRes + halfD, z + limitRes - halfD);

                    // Check against nearby lots to reduce sticking: simple grid spacing check
                    bool tooClose = false;
                    // check direct neighbors in grid (8-neighborhood)
                    for (int nx = -1; nx <= 1 && !tooClose; ++nx) {
                        for (int nz = -1; nz <= 1; ++nz) {
                            if (nx == 0 && nz == 0) continue;
                            float neighborCenterX = x + nx * 20.0f;
                            float neighborCenterZ = z + nz * 20.0f;
                            // If neighbor center is too close (distance less than half lot minus margin), mark tooClose
                            float dx = tryX - neighborCenterX;
                            float dz = tryZ - neighborCenterZ;
                            if (fabs(dx) < (halfW + 6.0f) && fabs(dz) < (halfD + 6.0f)) {
                                tooClose = true;
                                break;
                            }
                        }
                    }

                    if (tooClose) {
                        // shrink slightly and retry
                        width = glm::max(4.0f, width - 0.5f);
                        depth = glm::max(4.0f, depth - 0.5f);
                        halfW = width * 0.5f;
                        halfD = depth * 0.5f;
                        continue;
                    }

                    // Accept this placement
                    actualX = tryX;
                    actualZ = tryZ;
                    placed = true;
                }

                if (!placed) {
                    // As fallback, place at clamped center
                    actualX = glm::clamp(actualX, x - limitRes + halfW, x + limitRes - halfW);
                    actualZ = glm::clamp(actualZ, z - limitRes + halfD, z + limitRes - halfD);
                }

                // Render main building
                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(actualX, height / 2.0f, actualZ));
                model = glm::scale(model, glm::vec3(width, height, depth));
                buildingShader.SetMat4("model", model);
                buildingShader.SetVec3("objectColor", houseColors[colorIndex]);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // Add triangular roof if applicable
                if (hasTriangularRoof) {
                    float roofHeight = glm::clamp(glm::min(width, depth) * 0.35f, 1.0f, 4.0f);
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(actualX, height + roofHeight / 2.0f + 0.05f, actualZ));
                    model = glm::scale(model, glm::vec3(width * 1.02f, roofHeight, depth * 1.02f));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", roofColors[roofColorIndex]);
                    glBindVertexArray(roofVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 18); // roof VAO has 18 vertices
                    // restore cube vao for following draws
                    glBindVertexArray(cubeVAO);
                }

                // Add windows (front facade)
                int numWindows = (int)height / 3;
                for (int w = 0; w < numWindows; ++w) {
                    float winY = 1.0f + w * 2.5f;
                    float winWidth = width * 0.18f;
                    float winHeight = height * 0.18f;
                    float winZ = actualZ + depth / 2.0f + 0.01f;
                    float winX = actualX - width * 0.25f;
                    for (int i = 0; i < 2; ++i) {
                        model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(winX + i * width * 0.5f, winY, winZ));
                        model = glm::scale(model, glm::vec3(winWidth, winHeight, 0.05f));
                        buildingShader.SetMat4("model", model);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.6f, 0.8f, 0.95f)); // Glass blue
                        glBindVertexArray(cubeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }

                // Add door (front facade, ground level)
                float doorWidth = width * 0.22f;
                float doorHeight = height * 0.32f;
                float doorX = actualX;
                float doorY = doorHeight / 2.0f;
                float doorZ = actualZ + depth / 2.0f + 0.01f;
                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(doorX, doorY, doorZ));
                model = glm::scale(model, glm::vec3(doorWidth, doorHeight, 0.07f));
                buildingShader.SetMat4("model", model);
                buildingShader.SetVec3("objectColor", glm::vec3(0.7f, 0.5f, 0.3f)); // Brown door
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // Add balconies for apartments
                if (!hasTriangularRoof && height > 12.0f) {
                    int numBalconies = (int)height / 6;
                    for (int b = 0; b < numBalconies; ++b) {
                        float balY = 2.0f + b * 5.0f;
                        float balWidth = width * 0.5f;
                        float balDepth = 1.0f;
                        float balX = actualX;
                        float balZ = actualZ + depth / 2.0f + balDepth / 2.0f + 0.02f;
                        model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(balX, balY, balZ));
                        model = glm::scale(model, glm::vec3(balWidth, 0.2f, balDepth));
                        buildingShader.SetMat4("model", model);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.7f, 0.7f, 0.7f)); // Balcony gray
                        glBindVertexArray(cubeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }

                // Add rooftop details (air conditioner units)
                if (!hasTriangularRoof && (rand() % 2 == 0)) {
                    int numUnits = 1 + rand() % 2;
                    for (int u = 0; u < numUnits; ++u) {
                        float unitX = actualX + (width * 0.3f) * (u == 0 ? -1 : 1);
                        float unitY = height + 0.35f;
                        float unitZ = actualZ + depth * 0.2f;
                        model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(unitX, unitY, unitZ));
                        model = glm::scale(model, glm::vec3(0.7f, 0.35f, 0.7f));
                        buildingShader.SetMat4("model", model);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.85f, 0.85f, 0.85f)); // AC unit
                        glBindVertexArray(cubeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }

                // Add a small garage or shed occasionally, placed to the side so it doesn't touch main building
                if ((rand() % 100) < 20) { // 20% chance
                    float gW = glm::clamp(width * 0.5f, 2.0f, 5.0f);
                    float gD = glm::clamp(depth * 0.4f, 2.0f, 5.0f);
                    float gH = glm::clamp(height * 0.5f, 1.5f, 3.5f);
                    float side = ((rand() % 2) == 0) ? -1.0f : 1.0f;
                    float gx = actualX + side * (halfW + gW * 0.5f + 1.0f);
                    float gz = actualZ + ((rand() % 100) / 100.0f - 0.5f) * 2.0f;
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(gx, gH / 2.0f, gz));
                    model = glm::scale(model, glm::vec3(gW, gH, gD));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", glm::vec3(0.6f, 0.55f, 0.5f));
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    // small roof for garage
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(gx, gH + 0.6f, gz));
                    model = glm::scale(model, glm::vec3(gW * 1.02f, 0.6f, gD * 1.02f));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", glm::vec3(0.35f, 0.25f, 0.2f));
                    glBindVertexArray(roofVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 18);
                    glBindVertexArray(cubeVAO);
                }
            }
        }

        // draw skybox
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

    // Camera mode switching
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cameraKeyPressed) {
        cameraKeyPressed = true;

        // Cycle through camera modes
        static int currentMode = 0;
        currentMode = (currentMode + 1) % 3;  // 3 modes: FREE_FLY, FIRST_PERSON, ORBITAL

        switch (currentMode) {
        case 0:
            camera.SetCameraMode(Camera::FREE_FLY);
            std::cout << "Camera Mode: FREE FLY" << std::endl;
            break;
        case 1:
            camera.SetCameraMode(Camera::FIRST_PERSON);
            std::cout << "Camera Mode: FIRST PERSON" << std::endl;
            break;
        case 2:
            camera.SetCameraMode(Camera::ORBITAL);
            camera.SetOrbitTarget(glm::vec3(0.0f, 8.0f, 0.0f), 40.0f);
            std::cout << "Camera Mode: ORBITAL" << std::endl;
            break;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) {
        cameraKeyPressed = false;
    }

    // Reset camera
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camera.ResetToDefault();
        std::cout << "Camera Reset" << std::endl;
    }

    // Hot reload shaders (F5)
    static bool f5KeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS && !f5KeyPressed) {
        f5KeyPressed = true;
        std::cout << "Reloading shaders from files..." << std::endl;
        std::cout << "Shader hot-reload feature requires global shader reference" << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_RELEASE) {
        f5KeyPressed = false;
    }

    // Speed adjustment
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera.MovementSpeed = 25.0f;  // Fast movement for larger city
    }
    else {
        camera.MovementSpeed = 12.0f;  // Normal speed
    }

    // Light mode
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS && !lightKeyPressed) {
        lightKeyPressed = true;
        useDirectionalLight = !useDirectionalLight;

        if (useDirectionalLight) {
            std::cout << "Light mode: Directional" << std::endl;
        }
        else {
            std::cout << "Light mode: Point" << std::endl;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_RELEASE) {
        lightKeyPressed = false;
    }
}

GLuint createCube()
{
    // Cube vertices with normals and texture coordinates
    float vertices[] = {
        // positions          // normals           // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return VAO;
}

// Update car positions and handle traffic flow
void updateCars(float deltaTime) {
    for (auto it = cars.begin(); it != cars.end();) {
        Car& car = *it;

        // Move car forward
        car.position += car.direction * car.speed * deltaTime;

        // Remove cars that are too far away
        if (abs(car.position.x) > 150.0f || abs(car.position.z) > 150.0f) {
            it = cars.erase(it);
        }
        else {
            ++it;
        }
    }
}

// Spawn new cars at road entrances
void spawnCar() {
    if (cars.size() >= 40) return;  // Limit number of cars

    // Realistic car colors
    glm::vec3 carColors[] = {
        glm::vec3(0.1f, 0.1f, 0.1f),   // Black
        glm::vec3(0.9f, 0.9f, 0.9f),   // White
        glm::vec3(0.7f, 0.7f, 0.7f),   // Silver
        glm::vec3(0.8f, 0.1f, 0.1f),   // Red
        glm::vec3(0.1f, 0.3f, 0.8f),   // Blue
        glm::vec3(0.2f, 0.2f, 0.2f),   // Dark gray
        glm::vec3(0.6f, 0.3f, 0.1f),   // Brown/Bronze
        glm::vec3(0.1f, 0.5f, 0.2f),   // Dark green
        glm::vec3(0.8f, 0.8f, 0.1f),   // Yellow
        glm::vec3(0.5f, 0.1f, 0.5f)    // Purple
    };

    int colorIndex = rand() % 10;
    int roadType = rand() % 2;  // Focus on main highways
    int lane = rand() % 2;      // Choose lane
    int carType = rand() % 4;   // Choose car type (sedan, SUV, truck, hatchback)
    float speed = 18.0f + (rand() % 8) * 2.5f;  // 18-38 units/second

    glm::vec3 position, direction;

    switch (roadType) {
    case 0: // East-West Highway
        if (lane == 0) {  // Right lane (going East)
            position = glm::vec3(-120.0f, 0.8f, -6.0f);
            direction = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        else {  // Left lane (going West)
            position = glm::vec3(120.0f, 0.8f, 6.0f);
            direction = glm::vec3(-1.0f, 0.0f, 0.0f);
        }
        break;

    case 1: // North-South Highway
        if (lane == 0) {  // Right lane (going North)
            position = glm::vec3(6.0f, 0.8f, -120.0f);
            direction = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        else {  // Left lane (going South)
            position = glm::vec3(-6.0f, 0.8f, 120.0f);
            direction = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        break;
    }

    cars.emplace_back(position, direction, carColors[colorIndex], speed, lane, roadType, carType);
}

// Render road infrastructure (signs, lights, barriers)
void renderRoadInfrastructure(Shader& shader, GLuint cubeVAO, float currentTime) {
    glm::mat4 model;

    // === STREET LIGHTS ===
    // Highway street lights
    for (int i = -120; i <= 120; i += 30) {  // Every 30 units
        // Light poles on highway
        for (int side = -1; side <= 1; side += 2) {  // Both sides
            float zPos = side * 15.0f;  // 15 units from road center

            // Light pole
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(i, 5.0f, zPos));
            model = glm::scale(model, glm::vec3(0.3f, 10.0f, 0.3f));
            shader.SetMat4("model", model);
            shader.SetVec3("objectColor", glm::vec3(0.4f, 0.4f, 0.4f));  // Gray pole

            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Light fixture (with animated brightness)
            float brightness = 0.8f + 0.2f * sin(currentTime * 0.5f + i * 0.1f);
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(i, 9.5f, zPos));
            model = glm::scale(model, glm::vec3(1.0f, 0.8f, 1.0f));
            shader.SetMat4("model", model);
            shader.SetVec3("objectColor", glm::vec3(brightness, brightness, 0.9f));  // Cool light

            glDrawArrays(GL_TRIANGLES, 0, 36);

            // --- STREET LIGHT GLOW EFFECT ---
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(i, 9.5f, zPos));
            model = glm::scale(model, glm::vec3(2.2f, 1.8f, 2.2f)); // Larger, soft glow
            shader.SetMat4("model", model);
            shader.SetVec3("objectColor", glm::vec3(1.0f, 1.0f, 0.8f)); // Warm glow
            // If your shader supports alpha, set alpha here. Otherwise, use color intensity.
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glDisable(GL_BLEND);
        }
    }

    // Street lights on North-South highway
    for (int i = -120; i <= 120; i += 30) {
        if (i >= -15 && i <= 15) continue;  // Skip intersection

        for (int side = -1; side <= 1; side += 2) {
            float xPos = side * 15.0f;

            // Light pole
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(xPos, 5.0f, i));
            model = glm::scale(model, glm::vec3(0.3f, 10.0f, 0.3f));
            shader.SetMat4("model", model);
            shader.SetVec3("objectColor", glm::vec3(0.4f, 0.4f, 0.4f));

            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Light fixture
            float brightness = 0.8f + 0.2f * sin(currentTime * 0.5f + i * 0.1f);
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(xPos, 9.5f, i));
            model = glm::scale(model, glm::vec3(1.0f, 0.8f, 1.0f));
            shader.SetMat4("model", model);
            shader.SetVec3("objectColor", glm::vec3(brightness, brightness, 0.9f));

            glDrawArrays(GL_TRIANGLES, 0, 36);

            // --- STREET LIGHT GLOW EFFECT ---
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(xPos, 9.5f, i));
            model = glm::scale(model, glm::vec3(2.2f, 1.8f, 2.2f)); // Larger, soft glow
            shader.SetMat4("model", model);
            shader.SetVec3("objectColor", glm::vec3(1.0f, 1.0f, 0.8f)); // Warm glow
            // If your shader supports alpha, set alpha here. Otherwise, use color intensity.
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glDisable(GL_BLEND);
        }
    }

    // === ROAD SIGNS ===
    // Highway signs
    std::vector<glm::vec3> signPositions = {
        glm::vec3(-90.0f, 0.0f, -18.0f),  // Highway entrance signs
        glm::vec3(90.0f, 0.0f, 18.0f),
        glm::vec3(-18.0f, 0.0f, -90.0f),
        glm::vec3(18.0f, 0.0f, 90.0f),
        glm::vec3(-60.0f, 0.0f, -18.0f),  // Distance signs
        glm::vec3(60.0f, 0.0f, 18.0f),
    };

    for (const auto& signPos : signPositions) {
        // Sign post
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(signPos.x, 2.5f, signPos.z));
        model = glm::scale(model, glm::vec3(0.2f, 5.0f, 0.2f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.5f, 0.5f, 0.5f));  // Metal post

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Sign board (green highway sign)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(signPos.x, 4.0f, signPos.z));
        model = glm::scale(model, glm::vec3(4.0f, 1.5f, 0.1f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.1f, 0.6f, 0.1f));  // Highway green

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Sign text area (white)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(signPos.x, 4.0f, signPos.z + 0.05f));
        model = glm::scale(model, glm::vec3(3.5f, 1.0f, 0.02f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.9f));  // White text area

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // === HIGHWAY BARRIERS ===
    // Central barriers on highways
    for (int i = -120; i <= 120; i += 3) {
        // Skip intersection area
        if (i >= -20 && i <= 20) continue;

        // Concrete barrier on East-West highway
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(i, 0.8f, 0.0f));
        model = glm::scale(model, glm::vec3(3.0f, 1.6f, 0.5f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.7f, 0.7f, 0.7f));  // Concrete gray

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    for (int i = -120; i <= 120; i += 3) {
        if (i >= -20 && i <= 20) continue;

        // Concrete barrier on North-South highway  
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.8f, i));
        model = glm::scale(model, glm::vec3(0.5f, 1.6f, 3.0f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.7f, 0.7f, 0.7f));

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // === TRAFFIC SIGNALS ===
    // Traffic lights at major intersections
    std::vector<glm::vec3> trafficLightPositions = {
        glm::vec3(-20.0f, 0.0f, -20.0f),  // Four corners of main intersection
        glm::vec3(20.0f, 0.0f, -20.0f),
        glm::vec3(-20.0f, 0.0f, 20.0f),
        glm::vec3(20.0f, 0.0f, 20.0f)
    };

    for (const auto& tlPos : trafficLightPositions) {
        // Traffic light pole
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(tlPos.x, 4.0f, tlPos.z));
        model = glm::scale(model, glm::vec3(0.3f, 8.0f, 0.3f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.3f, 0.3f, 0.3f));

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Traffic light housing
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(tlPos.x, 7.0f, tlPos.z));
        model = glm::scale(model, glm::vec3(0.8f, 2.0f, 0.8f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.2f, 0.2f, 0.2f));

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Animated traffic lights (cycling through colors)
        float cycle = fmod(currentTime, 9.0f);  // 9-second cycle
        glm::vec3 lightColor;
        if (cycle < 3.0f) {
            lightColor = glm::vec3(0.9f, 0.1f, 0.1f);  // Red
        }
        else if (cycle < 4.0f) {
            lightColor = glm::vec3(0.9f, 0.9f, 0.1f);  // Yellow
        }
        else {
            lightColor = glm::vec3(0.1f, 0.9f, 0.1f);  // Green
        }

        // Active light
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(tlPos.x, 7.0f, tlPos.z + 0.5f));
        model = glm::scale(model, glm::vec3(0.4f, 0.4f, 0.1f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", lightColor);

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

// Stub implementation for unresolved external symbol
void renderTrees(Shader& shader, GLuint cubeVAO, GLuint cylinderVAO) {
    // TODO: Implement tree rendering logic
}

GLuint createGround()
{
    // Ground vertices (a large plane)
    float vertices[] = {
        // positions          // normals           // texture coords
        -1.0f, 0.0f, -1.0f,   0.0f,  1.0f, 0.0f,   0.0f, 0.0f,
         1.0f, 0.0f, -1.0f,   0.0f,  1.0f, 0.0f,   10.0f, 0.0f, // bump
         1.0f, 0.0f,  1.0f,   0.0f,  1.0f, 0.0f,   10.0f, 1.0f, // bump
        -1.0f, 0.0f,  1.0f,   0.0f,  1.0f, 0.0f,   0.0f, 1.0f,
    };

    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return VAO;
}

GLuint createTriangularRoof()
{
    // Triangular roof vertices (pitched roof)
    float vertices[] = {
        // Front triangle face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.0f,  0.5f,  0.0f,  0.0f,  0.0f,  1.0f,  0.5f, 1.0f,

         // Back triangle face
          0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
          0.0f,  0.5f,  0.0f,  0.0f,  0.0f, -1.0f,  0.5f, 1.0f,

          // Left slope face
          -0.5f, -0.5f, -0.5f, -0.707f,  0.707f,  0.0f,  0.0f, 0.0f,
          -0.5f, -0.5f,  0.5f, -0.707f,  0.707f,  0.0f,  1.0f, 0.0f,
           0.0f,  0.5f,  0.0f, -0.707f,  0.707f,  0.0f,  0.5f, 1.0f,

           // Right slope face
            0.5f, -0.5f,  0.5f,  0.707f,  0.707f,  0.0f,  0.0f, 0.0f,
            0.5f, -0.5f, -0.5f,  0.707f,  0.707f,  0.0f,  1.0f, 0.0f,
            0.0f,  0.5f,  0.0f,  0.707f,  0.707f,  0.0f,  0.5f, 1.0f,

            // Bottom face (inside of roof, usually not visible)
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,

             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return VAO;
}

GLuint createCylinder()
{
    const int segments = 16;
    const float radius = 0.5f;
    const float height = 1.0f;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    // Generate vertices for bottom circle
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        // Bottom vertex
        vertices.insert(vertices.end(), {x, -height/2.0f, z, 0.0f, -1.0f, 0.0f, (float)i/segments, 0.0f});
        // Top vertex
        vertices.insert(vertices.end(), {x, height/2.0f, z, 0.0f, 1.0f, 0.0f, (float)i/segments, 1.0f});
    }
    // Center vertices for caps
    vertices.insert(vertices.end(), {0.0f, -height/2.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f}); // Bottom center
    vertices.insert(vertices.end(), {0.0f, height/2.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f});  // Top center
    int bottomCenterIndex = (segments + 1) * 2;
    int topCenterIndex = bottomCenterIndex + 1;
    // Generate indices for cylinder sides
    for (int i = 0; i < segments; i++) {
        unsigned int bottom1 = static_cast<unsigned int>(i * 2);
        unsigned int top1 = static_cast<unsigned int>(i * 2 + 1);
        unsigned int bottom2 = static_cast<unsigned int>(((i + 1) % (segments + 1)) * 2);
        unsigned int top2 = static_cast<unsigned int>(((i + 1) % (segments + 1)) * 2 + 1);
        // Side triangles
        indices.insert(indices.end(), {bottom1, bottom2, top1});
        indices.insert(indices.end(), {top1, bottom2, top2});
        // Bottom cap
        indices.insert(indices.end(), {static_cast<unsigned int>(bottomCenterIndex), bottom2, bottom1});
        // Top cap  
        indices.insert(indices.end(), {static_cast<unsigned int>(topCenterIndex), top1, top2});
    }
    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    return VAO;
}

// Add renderRealisticCar function
void renderRealisticCar(const Car& car, Shader& shader, GLuint cubeVAO, GLuint cylinderVAO)
{
    glm::mat4 model;

    // Main body
    model = glm::mat4(1.0f);
    model = glm::translate(model, car.position);
    model = glm::scale(model, glm::vec3(car.width, car.height, car.length));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", car.color);

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Wheels
    float wheelRadius = car.width * 0.4f;
    float wheelWidth = car.width * 0.2f;
    glm::vec3 wheelColor = glm::vec3(0.2f);

    // Front wheels
    model = glm::mat4(1.0f);
    model = glm::translate(model, car.position + glm::vec3(-car.width / 2.0f, -car.height / 2.0f, car.length / 2.0f));
    model = glm::scale(model, glm::vec3(wheelRadius, wheelWidth, wheelRadius));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", wheelColor);

    glBindVertexArray(cylinderVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    model = glm::mat4(1.0f);
    model = glm::translate(model, car.position + glm::vec3(car.width / 2.0f, -car.height / 2.0f, car.length / 2.0f));
    model = glm::scale(model, glm::vec3(wheelRadius, wheelWidth, wheelRadius));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", wheelColor);

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    // Back wheels
    model = glm::mat4(1.0f);
    model = glm::translate(model, car.position + glm::vec3(-car.width / 2.0f, -car.height / 2.0f, -car.length / 2.0f));
    model = glm::scale(model, glm::vec3(wheelRadius, wheelWidth, wheelRadius));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", wheelColor);

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    model = glm::mat4(1.0f);
    model = glm::translate(model, car.position + glm::vec3(car.width / 2.0f, -car.height / 2.0f, -car.length / 2.0f));
    model = glm::scale(model, glm::vec3(wheelRadius, wheelWidth, wheelRadius));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", wheelColor);

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    // --- CAR LAMP GLOW EFFECTS ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // Headlamps (front corners)
    for (int side = -1; side <= 1; side += 2) {
        glm::vec3 lampPos = car.position + glm::vec3(side * car.width / 2.0f, 0.0f, car.length / 2.0f + 0.2f);
        model = glm::mat4(1.0f);
        model = glm::translate(model, lampPos);
        model = glm::scale(model, glm::vec3(0.4f, 0.2f, 0.6f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(1.0f, 1.0f, 0.7f)); // Headlamp glow

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    // Taillamps (rear corners)
    for (int side = -1; side <= 1; side += 2) {
        glm::vec3 lampPos = car.position + glm::vec3(side * car.width / 2.0f, 0.0f, -car.length / 2.0f - 0.2f);
        model = glm::mat4(1.0f);
        model = glm::translate(model, lampPos);
        model = glm::scale(model, glm::vec3(0.3f, 0.15f, 0.4f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(1.0f, 0.1f, 0.1f)); // Taillamp glow

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glDisable(GL_BLEND);
}

// Add renderShoppingMallComplex implementation
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
    for (const auto& pcar : parkedCars) {
        renderRealisticCar(pcar, shader, cubeVAO, cylinderVAO);
    }
}

// === PEDESTRIAN FUNCTIONS IMPLEMENTATION ===
void spawnPedestrians(int count) {
    pedestrians.clear();
    srand(98765); // fixed seed for reproducibility
    int spawned = 0;
    int attempts = 0;
    while (spawned < count && attempts < count * 10) {
        attempts++;
        float x = -80.0f + (rand() % 161);
        float z = -80.0f + (rand() % 161);
        // Avoid roads and mall area
        if ((x >= -10.0f && x <= 10.0f) || (z >= -10.0f && z <= 10.0f)) continue;
        if (fmod(fabs(x), 40.0f) < 1.0f || fmod(fabs(z), 40.0f) < 1.0f) continue;
        float dx = x - 60.0f; float dz = z - 20.0f;
        if (dx*dx + dz*dz < 40.0f*40.0f) continue;
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
        // Avoid roads and mall area
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

void renderPedestrian(const Pedestrian& p, Shader& shader, GLuint cubeVAO, GLuint /*cylinderVAO*/) {
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