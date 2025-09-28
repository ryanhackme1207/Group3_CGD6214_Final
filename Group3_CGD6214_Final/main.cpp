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

#include "SceneGraph.h"
#include "LODManager.h"
#include "SpatialPartition.h"

#include "Pedestrians.h"

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
GLuint createGround();
GLuint createTriangularRoof();
GLuint createCylinder();
void updateCars(float deltaTime);
void spawnCar();
void renderRoadInfrastructure(class Shader& shader, GLuint cubeVAO, float currentTime);
void renderRealisticCar(const struct Car& car, class Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);
void renderTrees(class Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);
void renderShoppingMallComplex(class Shader& shader, GLuint cubeVAO, GLuint cylinderVAO);

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
    std::cout << "=== LIGHT CONTROLS ===" << std::endl;
    std::cout << "L: Change current light mode" << std::endl;
    std::cout << "=======================" << std::endl;
    std::cout << "=== MSAA CONTROLS ===" << std::endl;
    std::cout << "M: Toggle MSAA on/off (Current: " << (MSAA > 0 ? "ON" : "OFF") << ")" << std::endl;
    std::cout << "=======================" << std::endl;
    

    if (MSAA > 0) {
        glEnable(GL_MULTISAMPLE);
    }

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
        // moved tree generation and rendering after buildings so we can avoid placing trees inside buildings
        // renderTrees(buildingShader, cubeVAO, cylinderVAO); // removed

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

        // Collect placed building boxes so trees can avoid them
        std::vector<glm::vec4> buildingBoxes; // x, z, halfWidth, halfDepth

        // === FIRST: ADD ICONIC TOWERS ===

        // KL Tower (Kuala Lumpur Tower) - 421m tall
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(25.0f, 210.5f, 25.0f));  // Half of 421m height
        model = glm::scale(model, glm::vec3(3.0f, 421.0f, 3.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.8f, 0.8f, 0.9f));  // Light metallic

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // (push KL tower footprint to buildingBoxes to avoid trees too close)
        buildingBoxes.emplace_back(glm::vec4(25.0f, 25.0f, 3.0f * 0.5f, 421.0f * 0.5f));

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

        // Create realistic residential neighborhoods (denser, varied heights)
        for (int x = -100; x <= 100; x += 10)  // higher density (every 10m)
        {
            for (int z = -100; z <= 100; z += 10)
            {
                // Avoid main highways and central avenues: keep generous margins
                if ((x >= -15 && x <= 15) || (z >= -15 && z <= 15)) continue;

                // Avoid secondary grid roads at multiples of 40 (wider margin)
                if (fmod(fabs((float)x), 40.0f) < 2.0f || fmod(fabs((float)z), 40.0f) < 2.0f) continue;

                // Avoid mall area and its parking approach
                float dxMall = x - 60.0f; float dzMall = z - 20.0f;
                if (dxMall*dxMall + dzMall*dzMall < 35.0f*35.0f) continue;

                // Small random offset to avoid strict grid look
                float xOffset = ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
                float zOffset = ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
                float actualX = x + xOffset;
                float actualZ = z + zOffset;

                // Choose building type based on distance to city center so center has taller buildings
                float distCenter = sqrtf((float)(x * x + z * z));
                int buildingType;
                int r = rand() % 100;
                if (distCenter < 40.0f) {
                    // City core - higher chance of mixed mid/high-rise
                    if (r < 20) buildingType = 4;       // high-rise
                    else if (r < 55) buildingType = 3;  // small apartment / mid-rise
                    else if (r < 80) buildingType = 2;  // townhouse
                    else buildingType = 1;              // semi-detached
                }
                else if (distCenter < 80.0f) {
                    // Inner suburbs - more apartments and townhouses
                    if (r < 10) buildingType = 4;
                    else if (r < 40) buildingType = 3;
                    else if (r < 75) buildingType = 2;
                    else buildingType = 0; // house
                }
                else {
                    // Outer suburbs - mostly houses
                    if (r < 50) buildingType = 0;
                    else if (r < 75) buildingType = 2;
                    else buildingType = 1;
                }

                float width, depth, height;
                bool hasTriangularRoof = false;

                switch (buildingType) {
                case 0: // single family house
                    width = 7.0f + (rand() % 4) * 1.5f;    // 7-13
                    depth = 8.0f + (rand() % 3) * 1.5f;    // 8-11
                    height = 5.5f + (rand() % 2) * 2.0f;   // 5.5-7.5 (1-2 stories)
                    hasTriangularRoof = true;
                    break;
                case 1: // semi-detached
                    width = 11.0f + (rand() % 3) * 1.5f;   // 11-14
                    depth = 8.0f + (rand() % 3) * 1.0f;    // 8-10
                    height = 7.0f + (rand() % 2) * 1.0f;   // 7-8
                    hasTriangularRoof = true;
                    break;
                case 2: // townhouse
                    width = 5.0f + (rand() % 2) * 1.0f;    // 5-6
                    depth = 10.0f + (rand() % 3) * 1.5f;   // 10-14
                    height = 8.0f + (rand() % 3) * 1.5f;   // 8-12
                    hasTriangularRoof = (rand() % 2) == 0;
                    break;
                case 3: // small apartment / mid-rise
                    width = 12.0f + (rand() % 4) * 2.0f;   // 12-20
                    depth = 10.0f + (rand() % 4) * 2.0f;   // 10-18
                    height = 12.0f + (rand() % 6) * 3.0f;  // 12-30 (3-10 stories)
                    hasTriangularRoof = false;
                    break;
                case 4: // high-rise / skyscraper (city center)
                    width = 12.0f + (rand() % 6) * 4.0f;   // 12-36
                    depth = 12.0f + (rand() % 6) * 4.0f;   // 12-36
                    height = 40.0f + (rand() % 20) * 10.0f; // 40-240m
                    hasTriangularRoof = false;
                    break;
                default:
                    width = 8.0f; depth = 10.0f; height = 6.0f;
                }

                // Clamp dimensions to reasonable limits so they fit in lots
                width = glm::min(width, 40.0f);
                depth = glm::min(depth, 40.0f);

                // Ensure building remains within lot boundaries (avoid overlap with roads)
                float lotHalfRes = 8.0f; // smaller lot due to higher density
                float paddingRes = 1.5f;
                float limitRes = lotHalfRes - paddingRes;
                actualX = glm::clamp(actualX, (float)x - limitRes + width*0.5f, (float)x + limitRes - width*0.5f);
                actualZ = glm::clamp(actualZ, (float)z - limitRes + depth*0.5f, (float)z + limitRes - depth*0.5f);

                // Check collision with already placed buildings to avoid overlap
                bool collidesWithExisting = false;
                for (const auto &bb : buildingBoxes) {
                    float bx = bb.x, bz = bb.y, halfWb = bb.z, halfDb = bb.w;
                    float halfW = width * 0.5f;
                    float halfD = depth * 0.5f;
                    float minDistX = halfW + halfWb + 0.5f; // small buffer
                    float minDistZ = halfD + halfDb + 0.5f;
                    if (fabs(actualX - bx) < minDistX && fabs(actualZ - bz) < minDistZ) { collidesWithExisting = true; break; }
                }
                if (collidesWithExisting) continue; // skip placement if it would overlap

                // Additional safety: ensure building has clearance from major roads and secondary roads
                float halfW = width * 0.5f;
                float halfD = depth * 0.5f;
                float clearance = 2.5f; // meters from road edge
                bool tooCloseToRoad = false;
                // main EW highway at z=0 spans approx +/-12.5; require clearance
                if (fabs(actualZ) - halfD < 15.0f + clearance) tooCloseToRoad = true;
                // main NS highway at x=0
                if (fabs(actualX) - halfW < 15.0f + clearance) tooCloseToRoad = true;
                // secondary grid roads at multiples of 40
                float modX = fmodf(fabs(actualX), 40.0f);
                float distToNearestGridX = glm::min(modX, 40.0f - modX);
                if (distToNearestGridX - halfW < 4.0f + clearance) tooCloseToRoad = true;
                float modZ = fmodf(fabs(actualZ), 40.0f);
                float distToNearestGridZ = glm::min(modZ, 40.0f - modZ);
                if (distToNearestGridZ - halfD < 4.0f + clearance) tooCloseToRoad = true;

                if (tooCloseToRoad) continue; // skip this placement

                // Render main building
                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(actualX, height / 2.0f, actualZ));
                model = glm::scale(model, glm::vec3(width, height, depth));
                buildingShader.SetMat4("model", model);

                // Choose color palette based on type
                glm::vec3 color;
                if (buildingType == 4) color = glm::vec3(0.6f, 0.6f, 0.68f);
                else if (buildingType == 3) color = glm::vec3(0.78f, 0.78f, 0.82f);
                else if (buildingType == 2) color = glm::vec3(0.8f, 0.7f, 0.6f);
                else color = houseColors[rand() % 8];

                buildingShader.SetVec3("objectColor", color);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // record building footprint for tree placement
                buildingBoxes.emplace_back(glm::vec4(actualX, actualZ, halfW, halfD));

                // Add triangular roof if applicable
                if (hasTriangularRoof) {
                    float roofHeight = glm::clamp(glm::min(width, depth) * 0.35f, 0.8f, 4.0f);
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(actualX, height + roofHeight / 2.0f + 0.05f, actualZ));
                    model = glm::scale(model, glm::vec3(width * 1.02f, roofHeight, depth * 1.02f));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", roofColors[rand() % 8]);
                    glBindVertexArray(roofVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 18); // roof VAO has 18 vertices
                    glBindVertexArray(cubeVAO);
                }

                // Optional details: windows & doors (simplified)
                if (height > 5.0f && buildingType != 4) {
                    int numWindowsY = glm::max(1, (int)(height / 3.0f));
                    float winW = glm::min(1.2f, width * 0.18f);
                    float winH = glm::min(1.2f, height * 0.12f);
                    for (int wy = 0; wy < numWindowsY; ++wy) {
                        float wyPos = 1.0f + wy * 2.5f;
                        for (int iw = 0; iw < 2; ++iw) {
                            model = glm::mat4(1.0f);
                            model = glm::translate(model, glm::vec3(actualX - width*0.25f + iw * width*0.5f, wyPos, actualZ + depth/2.0f + 0.01f));
                            model = glm::scale(model, glm::vec3(winW, winH, 0.05f));
                            buildingShader.SetMat4("model", model);
                            buildingShader.SetVec3("objectColor", glm::vec3(0.6f, 0.8f, 0.95f));
                            glDrawArrays(GL_TRIANGLES, 0, 36);
                        }
                    }
                }
            }
        }

        // --- TREE PLACEMENT & RENDERING ---
        // Initialize trees after all buildings (including extraBuildings) are placed so buildingBoxes contains
        // all footprints and trees won't be placed on roofs.
        struct Tree { glm::vec3 pos; float trunkH; float trunkR; float foliageSize; };
        static std::vector<Tree> trees;
        static bool treesInitialized = false;
        if (!treesInitialized) {
            treesInitialized = true;
            srand(54321);
            int desiredTrees = 80; // fewer trees as requested
            int attempts = 0;
            const float minTreeSpacing = 5.0f; // ensure spacing between trees
            while ((int)trees.size() < desiredTrees && attempts < desiredTrees * 60) {
                attempts++;
                float tx = -100.0f + (rand() % 201);
                float tz = -100.0f + (rand() % 201);

                // avoid main roads and sidewalks
                if (fabs(tx) < 22.0f || fabs(tz) < 22.0f) continue;

                // avoid secondary grid roads approximated margin
                float modTx = fmodf(fabs(tx), 40.0f);
                if (glm::min(modTx, 40.0f - modTx) < 10.0f) continue;
                float modTz = fmodf(fabs(tz), 40.0f);
                if (glm::min(modTz, 40.0f - modTz) < 10.0f) continue;

                // avoid mall area
                float dxm = tx - 60.0f; float dzm = tz - 20.0f;
                if (dxm*dxm + dzm*dzm < 40.0f*40.0f) continue;

                // tree size
                float trunkH = 3.0f + (rand() % 100) / 100.0f * 4.5f; // 3.0 - 7.5
                float trunkR = 0.25f + (rand() % 100) / 100.0f * 0.6f; // 0.25 - 0.85
                float foliage = 2.8f + (rand() % 100) / 100.0f * 3.5f; // 2.8 - 6.3

                // clearance from buildings (use buildingBoxes recorded earlier)
                float clearance = 3.0f;
                bool collides = false;
                for (auto &b : buildingBoxes) {
                    float bx = b.x, bz = b.y, halfW = b.z, halfD = b.w;
                    // consider foliage radius when checking collisions
                    if (fabs(tx - bx) < (halfW + foliage * 0.6f + clearance) && fabs(tz - bz) < (halfD + foliage * 0.6f + clearance)) {
                        collides = true; break;
                    }
                }
                if (collides) continue;

                // spacing to other trees
                bool tooClose = false;
                for (const auto &et : trees) {
                    float dx = tx - et.pos.x; float dz = tz - et.pos.z;
                    if (dx*dx + dz*dz < minTreeSpacing * minTreeSpacing) { tooClose = true; break; }
                }
                if (tooClose) continue;

                trees.push_back({glm::vec3(tx, 0.0f, tz), trunkH, trunkR, foliage});
            }
        }

        // Draw trees (trunk + foliage)
        for (const auto &tr : trees) {
            // trunk
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(tr.pos.x, tr.trunkH / 2.0f, tr.pos.z));
            model = glm::scale(model, glm::vec3(tr.trunkR, tr.trunkH, tr.trunkR));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.36f, 0.20f, 0.09f));
            glBindVertexArray(cylinderVAO);
            glDrawElements(GL_TRIANGLES, 16 * 12, GL_UNSIGNED_INT, 0);

            // foliage (cube blob)
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(tr.pos.x, tr.trunkH + tr.foliageSize / 2.0f, tr.pos.z));
            model = glm::scale(model, glm::vec3(tr.foliageSize, tr.foliageSize, tr.foliageSize));
            buildingShader.SetMat4("model", model);
            float g1 = 0.45f + ((float)((int)tr.foliageSize % 8)) / 24.0f;
            buildingShader.SetVec3("objectColor", glm::vec3(0.08f, g1, 0.08f));
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
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

    // MSAA switch
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !msaaKeyPressed) {
        msaaKeyPressed = true;

        if (MSAA > 0) {
            MSAA = 0;
            glDisable(GL_MULTISAMPLE);
            std::cout << "MSAA: OFF" << std::endl;
        }
        else {
            MSAA = 4;
            glEnable(GL_MULTISAMPLE);
            std::cout << "MSAA: ON (4x)" << std::endl;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE) {
        msaaKeyPressed = false;
    }
}

GLuint createCube()
{
    // Cube vertices with normals and texture coordinates
    static const float vertices[] = {
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

GLuint createGround()
{
    // Ground vertices (a large plane)
    static const float vertices[] = {
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
    static const float vertices[] = {
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

    // More realistic car composed of several boxes: chassis, cabin, hood, trunk, windows, bumpers
    float w = car.width;
    float h = car.height;
    float l = car.length;

    // Split into chassis and cabin vertically
    float chassisH = h * 0.55f;
    float cabinH = h - chassisH;

    // Longitudinal splits: hood, cabin, trunk
    float hoodL = l * 0.20f;
    float cabinL = l * 0.55f;
    float trunkL = l - hoodL - cabinL;

    // Base Y where car.position is the center: bottom at car.position.y - h/2
    float baseBottom = car.position.y - h * 0.5f;

    // Determine orientation from car.direction (rotate around Y)
    float yaw = 0.0f;
    if (glm::length(car.direction) > 0.001f) {
        glm::vec3 dir = glm::normalize(car.direction);
        yaw = atan2(dir.x, dir.z); // rotation to align model's +Z with direction
    }

    glm::mat4 baseModel = glm::mat4(1.0f);
    baseModel = glm::translate(baseModel, car.position);
    baseModel = glm::rotate(baseModel, yaw, glm::vec3(0.0f, 1.0f, 0.0f));

    // Chassis box (low, wide) - positions are local offsets from car.position
    glm::vec3 chassisLocal = glm::vec3(0.0f, -h * 0.5f + chassisH * 0.5f, 0.0f);
    model = baseModel;
    model = glm::translate(model, chassisLocal);
    model = glm::scale(model, glm::vec3(w, chassisH, l));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", car.color * 0.92f);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Cabin box (upper, narrower)
    glm::vec3 cabinLocal = glm::vec3(0.0f, -h * 0.5f + chassisH + cabinH * 0.5f, 0.0f);
    model = baseModel;
    model = glm::translate(model, cabinLocal);
    model = glm::scale(model, glm::vec3(w * 0.88f, cabinH * 0.95f, cabinL * 0.98f));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", car.color);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Hood (front) and trunk (rear) slight slopes approximated by boxes
    glm::vec3 hoodLocal = glm::vec3(0.0f, -h * 0.5f + chassisH * 0.5f + 0.08f, (l * 0.5f - hoodL * 0.5f));
    model = baseModel;
    model = glm::translate(model, hoodLocal);
    model = glm::scale(model, glm::vec3(w * 0.98f, chassisH * 0.6f, hoodL));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", car.color * 0.96f);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glm::vec3 trunkLocal = glm::vec3(0.0f, -h * 0.5f + chassisH * 0.5f + 0.05f, -(l * 0.5f - trunkL * 0.5f));
    model = baseModel;
    model = glm::translate(model, trunkLocal);
    model = glm::scale(model, glm::vec3(w * 0.98f, chassisH * 0.5f, trunkL));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", car.color * 0.96f);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Windows (front, side, rear) - slightly transparent look via bluish tint
    glm::vec3 windowColor = glm::vec3(0.35f, 0.55f, 0.75f);
    float winThickness = 0.04f;
    // Front windshield
    model = baseModel;
    glm::vec3 fwLocal = glm::vec3(0.0f, cabinLocal.y + cabinH * 0.05f, cabinL * 0.5f - winThickness * 0.5f);
    model = glm::translate(model, fwLocal);
    model = glm::scale(model, glm::vec3(w * 0.86f, cabinH * 0.35f, winThickness));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", windowColor);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    // Rear windshield
    model = baseModel;
    glm::vec3 rwLocal = glm::vec3(0.0f, cabinLocal.y + cabinH * 0.05f, -cabinL * 0.5f + winThickness * 0.5f);
    model = glm::translate(model, rwLocal);
    model = glm::scale(model, glm::vec3(w * 0.86f, cabinH * 0.35f, winThickness));
    shader.SetMat4("model", model);
    shader.SetVec3("objectColor", windowColor);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Side windows (left and right) as thin boxes
    for (int side = -1; side <= 1; side += 2) {
        model = baseModel;
        glm::vec3 swLocal = glm::vec3(side * (w * 0.5f - winThickness * 0.5f), cabinLocal.y, 0.0f);
        model = glm::translate(model, swLocal);
        model = glm::scale(model, glm::vec3(winThickness, cabinH * 0.78f, cabinL * 0.92f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", windowColor);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Wheels: better positioned and scaled
    float wheelRadius = glm::clamp(w * 0.22f, 0.25f, 0.6f);
    float wheelWidth = glm::max(0.18f, w * 0.22f);
    glm::vec3 wheelColor = glm::vec3(0.08f, 0.08f, 0.08f);
    float axleY = -h * 0.5f + wheelRadius; // local Y
    float axleOffsetX = w * 0.5f - wheelRadius * 0.6f;
    float axleFrontZ = l * 0.5f - hoodL * 0.4f;
    float axleBackZ = -l * 0.5f + trunkL * 0.4f;

    // Draw four wheels
    std::vector<glm::vec3> wheelLocalCenters = {
        glm::vec3(-axleOffsetX, axleY, axleFrontZ),
        glm::vec3(axleOffsetX, axleY, axleFrontZ),
        glm::vec3(-axleOffsetX, axleY, axleBackZ),
        glm::vec3(axleOffsetX, axleY, axleBackZ)
    };
    glBindVertexArray(cylinderVAO);
    for (const auto &lc : wheelLocalCenters) {
        model = baseModel;
        model = glm::translate(model, lc);
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0,0,1)); // align cylinder axis
        model = glm::scale(model, glm::vec3(wheelRadius, wheelWidth, wheelRadius));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", wheelColor);
        glDrawElements(GL_TRIANGLES, 16 * 12, GL_UNSIGNED_INT, 0);
    }

    // Bumpers / lights: small boxes
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // Headlamps
    for (int side = -1; side <= 1; side += 2) {
        glm::vec3 lampLocal = glm::vec3(side * (w * 0.33f), -h * 0.5f + chassisH * 0.4f, l * 0.5f + 0.05f);
        model = baseModel;
        model = glm::translate(model, lampLocal);
        model = glm::scale(model, glm::vec3(0.25f, 0.15f, 0.05f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(1.0f, 0.95f, 0.7f));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    // Taillamps
    for (int side = -1; side <= 1; side += 2) {
        glm::vec3 lampLocal = glm::vec3(side * (w * 0.33f), -h * 0.5f + chassisH * 0.4f, -l * 0.5f - 0.05f);
        model = baseModel;
        model = glm::translate(model, lampLocal);
        model = glm::scale(model, glm::vec3(0.22f, 0.12f, 0.05f));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(1.0f, 0.12f, 0.12f));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glDisable(GL_BLEND);
}