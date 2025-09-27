#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include "Camera.h"
#include "Shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

void main() {
    FragColor = texture(skybox, TexCoords);
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
    float width;
    float height;
    float length;
    int lane;  // 0 = right lane, 1 = left lane
    int roadType; // 0 = main road X, 1 = main road Z, 2 = highway X, 3 = highway Z
    int carType; // 0 = sedan, 1 = SUV, 2 = truck, 3 = hatchback

    Car(glm::vec3 pos, glm::vec3 dir, glm::vec3 col, float spd, int ln, int rt, int ct)
        : position(pos), direction(dir), color(col), speed(spd), lane(ln), roadType(rt), carType(ct) {

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
float carSpawnTimer = 0.0f;

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
        if (data) {
            std::cout << "Success! Dimensions: " << width << "x" << height << ", Channels: " << nrChannels << std::endl;

            GLenum format;
            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;

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
    while (!glfwWindowShouldClose(window))
    {
        // Per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Update cars
        carSpawnTimer += deltaTime;
        if (carSpawnTimer > 1.5f) {  // Spawn a car every 1.5 seconds for more traffic
            spawnCar();
            carSpawnTimer = 0.0f;
        }
        updateCars(deltaTime);

        // Update camera
        camera.UpdateSmoothMovement(deltaTime);
        camera.UpdateOrbitalCamera(deltaTime);
        camera.UpdateTransition(deltaTime);

        // Render
        glClearColor(0.6f, 0.8f, 1.0f, 1.0f);  // Sky blue background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        buildingShader.Use();

        // View/projection transformations using camera
        glm::mat4 projection = camera.GetProjectionMatrix((float)WIDTH / (float)HEIGHT);
        glm::mat4 view = camera.GetViewMatrix();
        buildingShader.SetMat4("projection", projection);
        buildingShader.SetMat4("view", view);

        // Set lighting uniforms (sun-like lighting)
        buildingShader.SetVec3("lightPos", glm::vec3(50.0f, 80.0f, 50.0f));
        buildingShader.SetVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.8f));  // Warm sunlight
        buildingShader.SetVec3("viewPos", camera.GetPosition());

        // Render ground (grass/concrete)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(250.0f, 1.0f, 250.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.4f, 0.6f, 0.3f));  // Grass green

        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // === RENDER HIGHWAY SYSTEM ===
        // Main East-West Highway (25m wide total)
        for (int x = -120; x <= 120; x += 5) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, 0.02f, 0.0f));
            model = glm::scale(model, glm::vec3(5.0f, 0.04f, 25.0f));  // 25m wide highway
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.25f, 0.25f, 0.25f));  // Dark asphalt

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
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", glm::vec3(0.25f, 0.25f, 0.25f));

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

                // === ADD OFFICE BUILDINGS IN CITY CENTER ===
                // Create downtown/business district near center
                if (abs(x) <= 40 && abs(z) <= 40 && (rand() % 100) < 30) {  // 30% chance for office buildings in center
                    // Office building dimensions
                    float width = 15.0f + (rand() % 4) * 5.0f;      // 15-30m wide
                    float depth = 15.0f + (rand() % 4) * 5.0f;      // 15-30m deep  
                    float height = 40.0f + (rand() % 8) * 15.0f;    // 40-145m tall (10-35+ stories)

                    // Very tall skyscrapers (rare)
                    if ((rand() % 100) < 5) {  // 5% chance for super tall
                        height = 150.0f + (rand() % 6) * 25.0f;     // 150-275m tall
                        width = 20.0f + (rand() % 3) * 8.0f;        // Wider base for stability
                        depth = 20.0f + (rand() % 3) * 8.0f;
                    }

                    int colorIndex = rand() % 8;

                    // Add some random variation in position
                    float xOffset = ((rand() % 100) / 100.0f - 0.5f) * 6.0f;
                    float zOffset = ((rand() % 100) / 100.0f - 0.5f) * 6.0f;
                    float actualX = x + xOffset;
                    float actualZ = z + zOffset;

                    // Office building colors (more modern/glass-like)
                    glm::vec3 officeColors[] = {
                        glm::vec3(0.7f, 0.8f, 0.9f),   // Light blue glass
                        glm::vec3(0.6f, 0.7f, 0.7f),   // Green-tinted glass
                        glm::vec3(0.8f, 0.8f, 0.8f),   // Silver/white
                        glm::vec3(0.5f, 0.5f, 0.6f),   // Dark glass
                        glm::vec3(0.7f, 0.7f, 0.8f),   // Blue-gray
                        glm::vec3(0.6f, 0.6f, 0.6f),   // Medium gray
                        glm::vec3(0.8f, 0.7f, 0.6f),   // Gold/bronze glass
                        glm::vec3(0.5f, 0.6f, 0.7f)    // Steel blue
                    };

                    // Render main office building
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(actualX, height / 2.0f, actualZ));
                    model = glm::scale(model, glm::vec3(width, height, depth));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", officeColors[colorIndex]);

                    glDrawArrays(GL_TRIANGLES, 0, 36);

                    // Add antenna/spire to some tall buildings
                    if (height > 80.0f && (rand() % 3) == 0) {  // 33% chance for tall buildings
                        float spireHeight = 8.0f + (rand() % 3) * 4.0f;  // 8-16m spire

                        model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(actualX, height + spireHeight / 2.0f, actualZ));
                        model = glm::scale(model, glm::vec3(1.0f, spireHeight, 1.0f));
                        buildingShader.SetMat4("model", model);
                        buildingShader.SetVec3("objectColor", glm::vec3(0.8f, 0.8f, 0.9f));  // Metallic

                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }

                    // Add architectural details for very tall buildings
                    if (height > 120.0f) {
                        // Add setback upper section (common in skyscrapers)
                        float upperWidth = width * 0.7f;
                        float upperDepth = depth * 0.7f;
                        float upperHeight = height * 0.25f;

                        model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(actualX, height + upperHeight / 2.0f, actualZ));
                        model = glm::scale(model, glm::vec3(upperWidth, upperHeight, upperDepth));
                        buildingShader.SetMat4("model", model);
                        buildingShader.SetVec3("objectColor", officeColors[colorIndex] * 0.9f);  // Slightly darker

                        glDrawArrays(GL_TRIANGLES, 0, 36);

                        // Crown/top section
                        if ((rand() % 2) == 0) {
                            float crownHeight = 5.0f;
                            model = glm::mat4(1.0f);
                            model = glm::translate(model, glm::vec3(actualX, height + upperHeight + crownHeight / 2.0f, actualZ));
                            model = glm::scale(model, glm::vec3(upperWidth + 2.0f, crownHeight, upperDepth + 2.0f));
                            buildingShader.SetMat4("model", model);
                            buildingShader.SetVec3("objectColor", glm::vec3(0.9f, 0.9f, 0.6f));  // Golden crown

                            glDrawArrays(GL_TRIANGLES, 0, 36);
                        }
                    }

                    continue;  // Skip residential building generation for this spot
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

                // Render main building
                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(actualX, height / 2.0f, actualZ));
                model = glm::scale(model, glm::vec3(width, height, depth));
                buildingShader.SetMat4("model", model);
                buildingShader.SetVec3("objectColor", houseColors[colorIndex]);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);

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
            }
        }

        // draw skybox
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxShader);

        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

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
}

GLuint createCube()
{
    // Cube vertices with normals and texture coordinates
    float vertices[] = {
        // positions          // normals           // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
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

GLuint createGround()
{
    // Ground vertices (a large plane)
    float vertices[] = {
        // positions          // normals           // texture coords
        -1.0f, 0.0f, -1.0f,   0.0f,  1.0f, 0.0f,   0.0f, 0.0f,
         1.0f, 0.0f, -1.0f,   0.0f,  1.0f, 0.0f,   1.0f, 0.0f,
         1.0f, 0.0f,  1.0f,   0.0f,  1.0f, 0.0f,   1.0f, 1.0f,
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

// Add createCylinder function
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

// Add renderTrees function
void renderTrees(Shader& shader, GLuint cubeVAO, GLuint cylinderVAO)
{
    glm::mat4 model;
    srand(54321); // Consistent tree placement
    std::vector<glm::vec3> treePositions = {
        glm::vec3(-70.0f, 0.0f, -70.0f), glm::vec3(-50.0f, 0.0f, -90.0f), glm::vec3(-30.0f, 0.0f, -60.0f),
        glm::vec3(10.0f, 0.0f, -80.0f), glm::vec3(50.0f, 0.0f, -70.0f), glm::vec3(70.0f, 0.0f, -90.0f),
        glm::vec3(-90.0f, 0.0f, 10.0f), glm::vec3(-70.0f, 0.0f, 30.0f), glm::vec3(-60.0f, 0.0f, 50.0f),
        glm::vec3(-80.0f, 0.0f, 70.0f), glm::vec3(-90.0f, 0.0f, 90.0f), glm::vec3(10.0f, 0.0f, 70.0f),
        glm::vec3(30.0f, 0.0f, 90.0f), glm::vec3(70.0f, 0.0f, 10.0f), glm::vec3(90.0f, 0.0f, 30.0f),
        glm::vec3(60.0f, 0.0f, -10.0f), glm::vec3(-10.0f, 0.0f, -50.0f), glm::vec3(-30.0f, 0.0f, -10.0f),
        glm::vec3(-70.0f, 0.0f, -30.0f), glm::vec3(-10.0f, 0.0f, 10.0f), glm::vec3(-50.0f, 0.0f, 30.0f)
    };
    for (const auto& pos : treePositions) {
        // Randomize trunk and foliage size
        float trunkHeight = 2.5f + (rand() % 5) * 0.5f; // 2.5-5m
        float trunkWidth = 0.3f + (rand() % 3) * 0.1f;  // 0.3-0.5m
        float foliageSize = 1.2f + (rand() % 5) * 0.4f; // 1.2-3.2m
        // Trunk
        model = glm::mat4(1.0f);
        model = glm::translate(model, pos + glm::vec3(0.0f, trunkHeight / 2.0f, 0.0f));
        model = glm::scale(model, glm::vec3(trunkWidth, trunkHeight, trunkWidth));
        shader.SetMat4("model", model);
        shader.SetVec3("objectColor", glm::vec3(0.55f, 0.27f, 0.07f)); // Brown trunk
        glBindVertexArray(cylinderVAO);
        glDrawElements(GL_TRIANGLES, 96, GL_UNSIGNED_INT, 0);
        // Foliage: 2-3 layers
        int layers = 2 + rand() % 2;
        for (int i = 0; i < layers; ++i) {
            float layerHeight = trunkHeight + 0.5f + i * (foliageSize * 0.5f);
            float layerSize = foliageSize * (1.0f - i * 0.2f);
            float greenShade = 0.6f + (rand() % 4) * 0.1f;
            model = glm::mat4(1.0f);
            model = glm::translate(model, pos + glm::vec3(0.0f, layerHeight, 0.0f));
            model = glm::scale(model, glm::vec3(layerSize, layerSize, layerSize));
            shader.SetMat4("model", model);
            shader.SetVec3("objectColor", glm::vec3(0.1f, greenShade, 0.1f));
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
}