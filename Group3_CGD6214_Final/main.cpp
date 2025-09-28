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
#include "Traffic.h" // pull in car structures and traffic functions

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

    // Ensure shader defaults to safe values and print active uniforms for debugging
    buildingShader.Use();
    buildingShader.SetFloat("bumpIntensity", 0.0f);
    buildingShader.PrintActiveUniforms();

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
    float startX = mallCenter.x - (cols / 2.0f - 0.5f) * spacingX;
    float startZ = mallCenter.z - (rows / 2.0f - 0.5f) * spacingZ - 30.0f; // place parking in front of mall
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float px = startX + c * spacingX + ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
            float pz = startZ + r * spacingZ + ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
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
        // Ensure procedural geometry uses no texture unless explicitly bound by a Mesh draw
        buildingShader.SetBool("hasTexture", false);

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
        // groundVAO has an element array buffer (EBO) with 6 indices; use it for indexed draw
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
                if (dxMall * dxMall + dzMall * dzMall < 35.0f * 35.0f) continue;

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
                case 0: // single family house - varied smaller sizes
                    width = 5.5f + (rand() % 50) / 10.0f;    // 5.5 - 10.5
                    depth = 6.5f + (rand() % 50) / 10.0f;    // 6.5 - 11.5
                    height = 4.5f + (rand() % 40) / 10.0f;   // 4.5 - 8.5
                    hasTriangularRoof = (rand() % 100) < 85; // most have roofs
                    break;
                case 1: // semi-detached
                    width = 9.0f + (rand() % 60) / 10.0f;   // 9 - 15
                    depth = 7.0f + (rand() % 40) / 10.0f;   // 7 - 11
                    height = 6.0f + (rand() % 50) / 10.0f;  // 6 - 11
                    hasTriangularRoof = (rand() % 100) < 75;
                    break;
                case 2: // townhouse
                    width = 4.5f + (rand() % 30) / 10.0f;    // 4.5 - 7.5
                    depth = 8.5f + (rand() % 60) / 10.0f;   // 8.5 - 14.5
                    height = 7.0f + (rand() % 60) / 10.0f;  // 7 - 13
                    hasTriangularRoof = (rand() % 100) < 50;
                    break;
                case 3: // small apartment / mid-rise
                    width = 10.0f + (rand() % 80) / 10.0f;   // 10 - 18
                    depth = 9.0f + (rand() % 80) / 10.0f;    // 9 - 17
                    height = 10.0f + (rand() % 100) / 10.0f; // 10 - 20
                    hasTriangularRoof = (rand() % 100) < 20; // some have terraces (no roof)
                    break;
                case 4: // high-rise / skyscraper (city center)
                    width = 10.0f + (rand() % 260) / 10.0f;   // 10 - 36
                    depth = 10.0f + (rand() % 260) / 10.0f;   // 10 - 36
                    height = 30.0f + (rand() % 210) / 1.0f;   // 30 - 240 (bigger variance)
                    hasTriangularRoof = (rand() % 100) < 10;
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
                actualX = glm::clamp(actualX, (float)x - limitRes + width * 0.5f, (float)x + limitRes - width * 0.5f);
                actualZ = glm::clamp(actualZ, (float)z - limitRes + depth * 0.5f, (float)z + limitRes - depth * 0.5f);

                // Check collision with existing buildings to avoid overlap
                bool collidesWithExisting = false;
                for (const auto& bb : buildingBoxes) {
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

                // Add facade details: windows, balconies, doors depending on building type
                {
                    // Determine window grid based on building size
                    int windowCols = glm::max(1, (int)(width / 2.5f));
                    int windowRows = glm::max(1, (int)(height / 3.0f));
                    float winW = (width * 0.7f) / (float)windowCols;
                    float winH = glm::min(1.2f, (height * 0.6f) / (float)windowRows);
                    float winDepth = 0.05f; // thin window pane

                    // Front face (+Z)
                    for (int r = 0; r < windowRows; ++r) {
                        for (int c = 0; c < windowCols; ++c) {
                            // skip some windows randomly for variety on small buildings
                            if ((buildingType <= 1) && ((r + c) % 5 == 0)) continue;
                            float wx = actualX - width * 0.5f + (c + 0.5f) * (width / windowCols);
                            float wz = actualZ + depth * 0.5f + 0.051f; // slightly in front
                            float wy = (r + 0.5f) * (height / windowRows);

                            glm::mat4 wmodel = glm::mat4(1.0f);
                            wmodel = glm::translate(wmodel, glm::vec3(wx, wy, wz));
                            wmodel = glm::scale(wmodel, glm::vec3(winW * 0.85f, winH * 0.85f, winDepth));
                            buildingShader.SetMat4("model", wmodel);
                            // choose glass color
                            if (buildingType == 4) buildingShader.SetVec3("objectColor", glm::vec3(0.45f, 0.6f, 0.75f));
                            else buildingShader.SetVec3("objectColor", glm::vec3(0.4f, 0.55f, 0.65f));
                            glBindVertexArray(cubeVAO);
                            glDrawArrays(GL_TRIANGLES, 0, 36);

                            // small frame
                            glm::mat4 fmodel = glm::mat4(1.0f);
                            fmodel = glm::translate(fmodel, glm::vec3(wx, wy, wz - 0.03f));
                            fmodel = glm::scale(fmodel, glm::vec3(winW * 0.9f, winH * 0.9f, 0.02f));
                            buildingShader.SetMat4("model", fmodel);
                            buildingShader.SetVec3("objectColor", glm::vec3(0.08f, 0.08f, 0.09f));
                            glDrawArrays(GL_TRIANGLES, 0, 36);
                        }
                    }

                    // Side faces (left/right) fewer windows for narrow sides
                    int sideCols = glm::max(1, (int)(depth / 3.0f));
                    for (int side = -1; side <= 1; side += 2) {
                        for (int r = 0; r < windowRows; ++r) {
                            for (int c = 0; c < sideCols; ++c) {
                                float sz = actualZ - depth * 0.5f + (c + 0.5f) * (depth / sideCols);
                                float sx = actualX + side * (width * 0.5f + 0.051f);
                                float wy = (r + 0.5f) * (height / windowRows);
                                glm::mat4 wmodel = glm::mat4(1.0f);
                                wmodel = glm::translate(wmodel, glm::vec3(sx, wy, sz));
                                wmodel = glm::scale(wmodel, glm::vec3(winDepth, winH * 0.85f, winW * 0.6f));
                                buildingShader.SetMat4("model", wmodel);
                                buildingShader.SetVec3("objectColor", glm::vec3(0.4f, 0.55f, 0.65f));
                                glBindVertexArray(cubeVAO);
                                glDrawArrays(GL_TRIANGLES, 0, 36);
                            }
                        }
                    }

                    // Doors and balconies for residential and townhouses
                    if (buildingType == 0 || buildingType == 1 || buildingType == 2) {
                        // door at front center
                        glm::mat4 dmodel = glm::mat4(1.0f);
                        dmodel = glm::translate(dmodel, glm::vec3(actualX, 0.9f, actualZ + depth * 0.5f + 0.02f));
                        dmodel = glm::scale(dmodel, glm::vec3(glm::min(1.2f, width * 0.4f), 1.8f, 0.06f));
                        buildingShader.SetMat4("model", dmodel);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.12f, 0.07f, 0.03f));
                        glBindVertexArray(cubeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);

                        // small balcony for townhouses
                        if (buildingType == 2 || buildingType == 1) {
                            int balconies = glm::max(1, windowRows / 2);
                            for (int b = 0; b < balconies; ++b) {
                                float by = 1.2f + b * 2.4f;
                                glm::mat4 bmodel = glm::mat4(1.0f);
                                bmodel = glm::translate(bmodel, glm::vec3(actualX + width * 0.5f + 0.02f, by, actualZ - depth * 0.15f));
                                bmodel = glm::rotate(bmodel, glm::radians(90.0f), glm::vec3(0, 1, 0));
                                bmodel = glm::scale(bmodel, glm::vec3(0.06f, 0.2f, glm::min(2.0f, width * 0.8f)));
                                buildingShader.SetMat4("model", bmodel);
                                buildingShader.SetVec3("objectColor", glm::vec3(0.15f, 0.15f, 0.15f));
                                glBindVertexArray(cubeVAO);
                                glDrawArrays(GL_TRIANGLES, 0, 36);
                            }
                        }
                    }

                    // High-rise glass strips for skyscrapers
                    if (buildingType == 4) {
                        int strips = glm::clamp((int)(width / 2.0f), 3, 12);
                        for (int s = 0; s < strips; ++s) {
                            float sx = actualX - width * 0.5f + (s + 0.5f) * (width / strips);
                            glm::mat4 stripModel = glm::mat4(1.0f);
                            stripModel = glm::translate(stripModel, glm::vec3(sx, height * 0.5f, actualZ + depth * 0.51f));
                            stripModel = glm::scale(stripModel, glm::vec3((width / strips) * 0.6f, height * 0.95f, 0.04f));
                            buildingShader.SetMat4("model", stripModel);
                            buildingShader.SetVec3("objectColor", glm::vec3(0.35f, 0.55f, 0.75f));
                            glBindVertexArray(cubeVAO);
                            glDrawArrays(GL_TRIANGLES, 0, 36);
                        }
                    }
                }

                // record building footprint for tree placement
                buildingBoxes.emplace_back(glm::vec4(actualX, actualZ, halfW, halfD));

                // Render triangular roof if applicable
                if (hasTriangularRoof) {
                    // Choose a roof height relative to building height with a bit of randomness for variety
                    float roofHeight = glm::min(height * 0.6f + ((rand() % 100) / 100.0f) * 2.0f, 8.0f);
                    // Position roof so its base sits on top of the building (building top is at y = height)
                    float roofY = height + roofHeight * 0.5f;
                    glm::mat4 roofModel = glm::mat4(1.0f);
                    roofModel = glm::translate(roofModel, glm::vec3(actualX, roofY, actualZ));
                    // Slightly overhang the edges for realism
                    float overhang = 1.02f;
                    roofModel = glm::scale(roofModel, glm::vec3(width * overhang, roofHeight, depth * overhang));
                    buildingShader.SetMat4("model", roofModel);
                    // Pick a roof color from available palette
                    glm::vec3 roofColor = roofColors[rand() % 8];
                    buildingShader.SetVec3("objectColor", roofColor);
                    glBindVertexArray(roofVAO);
                    // createTriangularRoof provides 18 vertices (6 triangles)
                    glDrawArrays(GL_TRIANGLES, 0, 18);
                }

                // Small decorative tree near building (random chance)
                if ((rand() % 100) < 35) { // 35% chance
                    // Attempt to place a small tree beside the building
                    bool placedSmallTree = false;
                    for (int ttry = 0; ttry < 6 && !placedSmallTree; ++ttry) {
                        float angle = (rand() % 360) * (M_PI / 180.0f);
                        float dist = (halfW + 1.0f) + (rand() % 100) / 100.0f * 2.0f; // just outside building
                        float sx = actualX + cos(angle) * dist;
                        float sz = actualZ + sin(angle) * dist;
                        // avoid main roads and mall
                        if (fabs(sx) < 15.0f || fabs(sz) < 15.0f) continue;
                        float dxm2 = sx - 60.0f; float dzm2 = sz - 20.0f;
                        if (dxm2 * dxm2 + dzm2 * dzm2 < 35.0f * 35.0f) continue;
                        // check collision with existing building footprints
                        bool coll = false;
                        for (const auto& bb2 : buildingBoxes) {
                            float bx = bb2.x, bz = bb2.y, bHalfW = bb2.z, bHalfD = bb2.w;
                            if (fabs(sx - bx) < (0.8f + bHalfW + 0.5f) && fabs(sz - bz) < (0.8f + bHalfD + 0.5f)) { coll = true; break; }
                        }
                        if (coll) continue;
                        // Place small tree: render trunk + foliage and add tiny footprint
                        float trunkH = 1.0f + (rand() % 100) / 100.0f * 0.8f; // 1.0 - 1.8
                        float trunkR = 0.12f + (rand() % 100) / 100.0f * 0.18f; // 0.12 - 0.3
                        float foliage = 0.8f + (rand() % 100) / 100.0f * 0.8f; // 0.8 - 1.6
                        // trunk
                        model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(sx, trunkH / 2.0f, sz));
                        model = glm::scale(model, glm::vec3(trunkR, trunkH, trunkR));
                        buildingShader.SetMat4("model", model);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.36f, 0.20f, 0.09f));
                        glBindVertexArray(cylinderVAO);
                        glDrawElements(GL_TRIANGLES, 16 * 12, GL_UNSIGNED_INT, 0);
                        // foliage
                        model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(sx, trunkH + foliage / 2.0f, sz));
                        model = glm::scale(model, glm::vec3(foliage, foliage, foliage));
                        buildingShader.SetMat4("model", model);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.1f, 0.55f, 0.12f));
                        glBindVertexArray(cubeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);

                        // reserve small footprint so future buildings avoid it
                        buildingBoxes.emplace_back(glm::vec4(sx, sz, foliage * 0.5f + 0.4f, foliage * 0.5f + 0.4f));
                        placedSmallTree = true;
                    }
                }
            }
        }

        // === ADDITIONAL: DENSE CENTRAL BUSINESS DISTRICT (CBD) ===
        {
            // Create many high-rises around the city center (0,0)
            float cbdRadius = 35.0f;
            float roadClearance = 6.0f; // meters clearance from highways
            float highwayHalf = 12.5f; // half-width of main highways

            for (float gx = -cbdRadius; gx <= cbdRadius; gx += 6.0f) {
                for (float gz = -cbdRadius; gz <= cbdRadius; gz += 6.0f) {
                    float dist = sqrt(gx * gx + gz * gz);
                    if (dist > cbdRadius) continue;
                    float jitterX = ((rand() % 100) / 100.0f - 0.5f) * 2.5f;
                    float jitterZ = ((rand() % 100) / 100.0f - 0.5f) * 2.5f;
                    float bx = gx + jitterX;
                    float bz = gz + jitterZ;

                    // skip positions too close to main highways (EW at z=0, NS at x=0)
                    // we'll compute building extents below and test clearance before placing

                    // skip mall area
                    float dxm = bx - 60.0f; float dzm = bz - 20.0f;
                    if (dxm * dxm + dzm * dzm < 35.0f * 35.0f) continue;

                    // random building parameters biased to tall
                    float bWidth = 8.0f + (rand() % 220) / 10.0f;   // 8 - 30
                    float bDepth = 8.0f + (rand() % 220) / 10.0f;   // 8 - 30
                    float bHeight = 40.0f + (rand() % 400) / 10.0f;  // 40 - 440
                    bWidth = glm::min(bWidth, 36.0f);
                    bDepth = glm::min(bDepth, 36.0f);
                    bHeight = glm::min(bHeight, 320.0f);

                    float halfW = bWidth * 0.5f;
                    float halfD = bDepth * 0.5f;

                    // Ensure clearance from main highways and secondary grid roads
                    bool tooCloseToRoad = false;
                    if (fabs(bz) - halfD < highwayHalf + roadClearance) tooCloseToRoad = true; // too close to EW highway
                    if (fabs(bx) - halfW < highwayHalf + roadClearance) tooCloseToRoad = true; // too close to NS highway
                    // secondary grid roads at multiples of 40
                    float modX = fmodf(fabs(bx), 40.0f);
                    float distToNearestGridX = glm::min(modX, 40.0f - modX);
                    if (distToNearestGridX - halfW < 4.0f + roadClearance) tooCloseToRoad = true;
                    float modZ = fmodf(fabs(bz), 40.0f);
                    float distToNearestGridZ = glm::min(modZ, 40.0f - modZ);
                    if (distToNearestGridZ - halfD < 4.0f + roadClearance) tooCloseToRoad = true;

                    if (tooCloseToRoad) continue;

                    // collision with existing footprints
                    bool coll = false;
                    for (const auto& bb : buildingBoxes) {
                        float bx2 = bb.x, bz2 = bb.y, hw2 = bb.z, hd2 = bb.w;
                        if (fabs(bx - bx2) < (halfW + hw2 + 1.0f) && fabs(bz - bz2) < (halfD + hd2 + 1.0f)) { coll = true; break; }
                    }
                    if (coll) continue;

                    // render skyscraper
                    glm::mat4 bmodel = glm::mat4(1.0f);
                    bmodel = glm::translate(bmodel, glm::vec3(bx, bHeight / 2.0f, bz));
                    bmodel = glm::scale(bmodel, glm::vec3(bWidth, bHeight, bDepth));
                    buildingShader.SetMat4("model", bmodel);
                    // glassy/steel color
                    glm::vec3 bcolor = glm::vec3(0.55f, 0.58f, 0.68f) + glm::vec3(((rand() % 100) / 100.0f - 0.5f) * 0.06f);
                    buildingShader.SetVec3("objectColor", bcolor);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    // add vertical glass strips for visual variety
                    int strips = glm::clamp((int)(bWidth / 2.0f), 4, 20);
                    for (int s = 0; s < strips; ++s) {
                        float sx = bx - bWidth * 0.5f + (s + 0.5f) * (bWidth / strips);
                        glm::mat4 stripModel = glm::mat4(1.0f);
                        stripModel = glm::translate(stripModel, glm::vec3(sx, bHeight * 0.5f, bz + bDepth * 0.51f));
                        stripModel = glm::scale(stripModel, glm::vec3((bWidth / strips) * 0.6f, bHeight * 0.95f, 0.05f));
                        buildingShader.SetMat4("model", stripModel);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.32f, 0.5f, 0.7f));
                        glBindVertexArray(cubeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }

                    // rooftop helipad or mechanical top block
                    if ((rand() % 100) < 30) {
                        glm::mat4 top = glm::mat4(1.0f);
                        top = glm::translate(top, glm::vec3(bx, bHeight + 2.0f, bz));
                        top = glm::scale(top, glm::vec3(glm::min(bWidth, 8.0f), 4.0f, glm::min(bDepth, 8.0f)));
                        buildingShader.SetMat4("model", top);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.18f, 0.18f, 0.18f));
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }

                    // record footprint
                    buildingBoxes.emplace_back(glm::vec4(bx, bz, halfW, halfD));
                }
            }
        }

        // === ADDITIONAL: COMMERCIAL STRIPS (mid-rise offices/shops) ===
        {
            // along two parallel avenues at z = +/-40 create continuous commercial strips
            float avenues[2] = { 40.0f, -40.0f };
            float highwayHalf = 12.5f;
            float roadClearance = 4.0f;
            for (int ai = 0; ai < 2; ++ai) {
                float az = avenues[ai];
                for (float ax = -110.0f; ax <= 110.0f; ax += 12.0f) {
                    float jitter = ((rand() % 100) / 100.0f - 0.5f) * 3.0f;
                    float bx = ax + jitter;
                    float bz = az + ((rand() % 100) / 100.0f - 0.5f) * 2.0f;

                    // avoid mall area
                    float dxm = bx - 60.0f; float dzm = bz - 20.0f;
                    if (dxm * dxm + dzm * dzm < 35.0f * 35.0f) continue;

                    float bWidth = 10.0f + (rand() % 60) / 10.0f; // 10-16
                    float bDepth = 6.0f + (rand() % 40) / 10.0f; //6-10
                    float bHeight = 8.0f + (rand() % 120) / 10.0f; //8-20
                    float halfW = bWidth * 0.5f, halfD = bDepth * 0.5f;

                    // Ensure commercial buildings aren't built on main highways
                    bool tooCloseToRoad = false;
                    if (fabs(bz) - halfD < highwayHalf + roadClearance) tooCloseToRoad = true;
                    if (fabs(bx) - halfW < highwayHalf + roadClearance) tooCloseToRoad = true;
                    float modX = fmodf(fabs(bx), 40.0f);
                    float distToNearestGridX = glm::min(modX, 40.0f - modX);
                    if (distToNearestGridX - halfW < 4.0f + roadClearance) tooCloseToRoad = true;
                    if (tooCloseToRoad) continue;

                    bool coll = false;
                    for (const auto& bb : buildingBoxes) {
                        float bx2 = bb.x, bz2 = bb.y, hw2 = bb.z, hd2 = bb.w;
                        if (fabs(bx - bx2) < (halfW + hw2 + 0.8f) && fabs(bz - bz2) < (halfD + hd2 + 0.8f)) { coll = true; break; }
                    }
                    if (coll) continue;

                    glm::mat4 bmodel = glm::mat4(1.0f);
                    bmodel = glm::translate(bmodel, glm::vec3(bx, bHeight / 2.0f, bz));
                    bmodel = glm::scale(bmodel, glm::vec3(bWidth, bHeight, bDepth));
                    buildingShader.SetMat4("model", bmodel);
                    glm::vec3 bcolor = glm::vec3(0.78f, 0.76f, 0.72f) + glm::vec3(((rand() % 100) / 100.0f - 0.5f) * 0.06f);
                    buildingShader.SetVec3("objectColor", bcolor);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    // Front glass
                    glm::mat4 f = glm::mat4(1.0f);
                    f = glm::translate(f, glm::vec3(bx, bHeight * 0.5f - 0.5f, bz + bDepth * 0.51f));
                    f = glm::scale(f, glm::vec3(bWidth * 0.8f, bHeight * 0.6f, 0.04f));
                    buildingShader.SetMat4("model", f);
                    buildingShader.SetVec3("objectColor", glm::vec3(0.45f, 0.6f, 0.75f));
                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    // signage
                    if ((rand() % 100) < 40) {
                        glm::mat4 s = glm::mat4(1.0f);
                        s = glm::translate(s, glm::vec3(bx, bHeight - 0.6f, bz + bDepth * 0.52f));
                        s = glm::scale(s, glm::vec3(bWidth * 0.6f, 0.6f, 0.02f));
                        buildingShader.SetMat4("model", s);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.12f, 0.4f, 0.85f));
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }

                    buildingBoxes.emplace_back(glm::vec4(bx, bz, halfW, halfD));
                }
            }
        }

        // Call residential decorative lights after buildings are placed
        renderResidentialLights(buildingShader, cubeVAO, cylinderVAO, buildingBoxes, (float)glfwGetTime());

        // === Place and render a Bugatti showcase if available ===
        {
            static bool bugattiInitialized = false;
            static bool bugattiLoaded = false;
            static Model bugattiModel;
            static glm::vec3 bugattiPos = glm::vec3(0.0f);
            static float bugattiScale = 1.0f;

            if (!bugattiInitialized) {
                bugattiInitialized = true;

                // Try to find an OBJ exported from your Blender file in the 3D/source folder.
                // Expected source .blend: "3D/source/Bugatti Chiron Super sports Ske.blend"
                // Please export it from Blender to OBJ and place it alongside the .blend.
                const char* candidates[] = {

                    "3D/source/visiongt.obj",


                    // Also check common alternate location where you may have placed the exported OBJ and textures

                };

                for (const char* p : candidates) {
                    if (bugattiModel.LoadOBJ(p)) {
                        std::cout << "Loaded Bugatti model from: " << p << std::endl;
                        bugattiLoaded = true;
                        break;
                    }
                }
                if (!bugattiLoaded) {
                    std::cout << "Bugatti OBJ not found in expected locations." << std::endl;
                    std::cout << "Please open: C:\\Project\\Group3_CGD6214_Final\\Group3_CGD6214_Final\\3D\\source\\Bugatti Chiron Super sports Ske.blend in Blender and export it as OBJ or place the exported OBJ in 3D/textures." << std::endl;
                    std::cout << "  File -> Export -> Wavefront (.obj) -> choose output file name 'Bugatti Chiron Super sports Ske.obj' and place it in 3D/source/ or 3D/textures/" << std::endl;
                    std::cout << "Then re-run the application; the loader will attempt to load '3D/source/Bugatti Chiron Super sports Ske.obj', '3D/bugatti.obj' or files in 3D/textures/" << std::endl;
                }

                // If loaded, search for a free placement area
                if (bugattiLoaded) {
                    float searchMin = -80.0f, searchMax = 80.0f;
                    float step = 4.0f;
                    float carHalfW = 3.0f;   // required half-width of showroom area in meters
                    float carHalfD = 5.0f;   // required half-depth
                    float clearance = 4.0f;  // extra clearance from buildings/roads
                    float highwayHalf = 12.5f; // half width of main highway

                    bool found = false;
                    for (float tx = searchMin; tx <= searchMax && !found; tx += step) {
                        for (float tz = searchMin; tz <= searchMax && !found; tz += step) {
                            // avoid main highways (EW at z=0, NS at x=0)
                            if (fabs(tz) - carHalfD < highwayHalf + clearance) continue;
                            if (fabs(tx) - carHalfW < highwayHalf + clearance) continue;

                            // avoid secondary grid roads at multiples of 40
                            float modX = fmodf(fabs(tx), 40.0f);
                            float distToNearestGridX = glm::min(modX, 40.0f - modX);
                            if (distToNearestGridX - carHalfW < 4.0f + clearance) continue;
                            float modZ = fmodf(fabs(tz), 40.0f);
                            float distToNearestGridZ = glm::min(modZ, 40.0f - modZ);
                            if (distToNearestGridZ - carHalfD < 4.0f + clearance) continue;

                            // avoid mall area
                            float dxm = tx - 60.0f; float dzm = tz - 20.0f;
                            if (dxm * dxm + dzm * dzm < 40.0f * 40.0f) continue;

                            // check collision with all building footprints
                            bool coll = false;
                            for (const auto& bb : buildingBoxes) {
                                float bx = bb.x, bz = bb.y, hw = bb.z, hd = bb.w;
                                if (fabs(tx - bx) < (hw + carHalfW + clearance) && fabs(tz - bz) < (hd + carHalfD + clearance)) { coll = true; break; }
                            }
                            if (coll) continue;

                            // position is free
                            bugattiPos = glm::vec3(tx, 0.0f, tz);
                            // choose a reasonable scale — may need tuning depending on OBJ size
                            bugattiScale = 0.02f; // reduced scale so OBJ appears smaller; adjust if needed
                            found = true;
                        }
                    }
                    if (!found) {
                        std::cout << "No suitable free space found for Bugatti showcase. Consider increasing search area or reducing clearance." << std::endl;
                    }
                    else {
                        std::cout << "Bugatti will be placed at: (" << bugattiPos.x << ", " << bugattiPos.y << ", " << bugattiPos.z << ") scale=" << bugattiScale << std::endl;
                    }
                }
            }

            // Render the model if loaded and placed
            if (bugattiLoaded) {
                // Save previous objectColor
                glm::vec3 prevColor = glm::vec3(1.0f);
                // Try to read previous color from shader if available (best-effort)
                // We'll apply a Bugatti blue tint while drawing the model
                glm::vec3 bugColor = glm::vec3(0.02f, 0.18f, 0.45f); // deep Bugatti blue

                glm::mat4 bmodel = glm::mat4(1.0f);
                // lift slightly above ground if model origin at center
                bmodel = glm::translate(bmodel, glm::vec3(bugattiPos.x, 0.5f, bugattiPos.z));
                bmodel = glm::scale(bmodel, glm::vec3(bugattiScale));

                // Apply tint by temporarily setting objectColor uniform
                buildingShader.SetVec3("objectColor", bugColor);

                // Create a simple showcase platform and visual spotlights for the Bugatti
                // Platform (dark circular/rect base)
                glm::mat4 platformModel = glm::mat4(1.0f);
                platformModel = glm::translate(platformModel, glm::vec3(bugattiPos.x, 0.02f, bugattiPos.z));
                // scaled as a flat stage; adjust scale for your model size
                platformModel = glm::scale(platformModel, glm::vec3(6.0f, 0.04f, 10.0f));
                buildingShader.SetMat4("model", platformModel);
                buildingShader.SetVec3("objectColor", glm::vec3(0.06f, 0.06f, 0.07f));
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // Platform rim (slightly elevated reflective band)
                glm::mat4 rim = glm::mat4(1.0f);
                rim = glm::translate(rim, glm::vec3(bugattiPos.x, 0.06f, bugattiPos.z));
                rim = glm::scale(rim, glm::vec3(6.5f, 0.02f, 10.5f));
                buildingShader.SetMat4("model", rim);
                buildingShader.SetVec3("objectColor", glm::vec3(0.18f, 0.18f, 0.20f));
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // Visual spotlights: additive blended ground blobs + translucent cones above
                glEnable(GL_BLEND);
                // additive so lights brighten underlying objects
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);

                const int NUM_SHOW_SPOTS = 3;
                for (int i = 0; i < NUM_SHOW_SPOTS; ++i) {
                    float ang = i * 2.0f * 3.14159265f / NUM_SHOW_SPOTS + 0.3f;
                    float dx = cos(ang) * 3.0f;
                    float dz = sin(ang) * 2.5f;
                    // ground blob
                    glm::mat4 spotG = glm::mat4(1.0f);
                    spotG = glm::translate(spotG, glm::vec3(bugattiPos.x + dx, 0.03f, bugattiPos.z + dz));
                    // scale horizontal ellipse
                    float rx = 1.8f; float rz = 2.2f;
                    spotG = glm::scale(spotG, glm::vec3(rx, 0.01f, rz));
                    buildingShader.SetMat4("model", spotG);
                    // warm spotlight color
                    buildingShader.SetVec3("objectColor", glm::vec3(1.0f, 0.95f, 0.8f) * 0.9f);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    // translucent cone (approximate using tall thin cylinder) above to hint at light source
                    glm::mat4 cone = glm::mat4(1.0f);
                    cone = glm::translate(cone, glm::vec3(bugattiPos.x + dx, 6.5f, bugattiPos.z + dz));
                    // tall, slightly flared
                    cone = glm::scale(cone, glm::vec3(0.6f, 7.0f, 0.6f));
                    buildingShader.SetMat4("model", cone);
                    // lower alpha to make cone see-through
                    buildingShader.SetVec3("objectColor", glm::vec3(1.0f, 0.96f, 0.85f) * 0.25f);
                    glBindVertexArray(cylinderVAO);
                    // cylinder uses element array
                    glDrawElements(GL_TRIANGLES, 16 * 12, GL_UNSIGNED_INT, 0);
                }

                // restore blending state to default multiplicative alpha for rest of scene
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_BLEND);

                bugattiModel.Draw(buildingShader, bmodel);

                // Restore previous object color
                buildingShader.SetVec3("objectColor", prevColor);
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

        -0.5f,  0.5f,  0.5f, -1.0f,  0.707f,  0.707f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.707f,  0.707f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.707f,  0.707f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.707f,  0.707f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.707f,  0.707f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.707f,  0.707f,  1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.707f,  0.707f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.707f,  0.707f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.707f,  0.707f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.707f,  0.707f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.707f,  0.707f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.707f,  0.707f,  1.0f, 0.0f,

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