#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <functional>

#include "Shader.h"
#include "AdvancedCamera.h"
#include "CityManager.h"
#include "LightingSystem.h"
#include "MaterialSystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Window settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
const float ASPECT_RATIO = (float)SCR_WIDTH / (float)SCR_HEIGHT;

// Global variables
std::unique_ptr<AdvancedCamera> camera;
std::unique_ptr<LightingSystem> lightingSystem;
std::unique_ptr<MaterialManager> materialManager;
std::unique_ptr<CityManager> cityManager;

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
unsigned int cubeVAO = 0;
unsigned int skyboxVAO = 0;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool keys[1024];
bool keyPressed[1024];

bool enableHDR = true;
bool enableBloom = true;
bool enableSSAO = true;
bool enableShadows = true;
bool enableWireframe = false;
float exposure = 1.0f;
bool showUI = true;

struct PerformanceStats {
    float frameTime = 0.0f;
    int fps = 0;
    int frameCount = 0;
    float fpsTimer = 0.0f;
    int visibleObjects = 0;
    int totalObjects = 0;
} perfStats;

// Framebuffer objects
unsigned int hdrFBO, hdrColorBuffer, hdrDepthBuffer;
unsigned int quadVAO, quadVBO;

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow* window);
void updatePerformanceStats();
void renderUI();
void setupFramebuffers();
void renderScene(unsigned int shaderProgram, bool shadowPass = false);
void renderPostProcessing(Shader& hdrShader);
unsigned int loadCubemap(std::vector<std::string> faces);
unsigned int createProceduralSkybox();

int main() {
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Advanced 3D City Simulation", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Configure OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Initialize systems
    camera = std::make_unique<AdvancedCamera>(glm::vec3(0.0f, 25.0f, 50.0f));
    camera->SetMode(CameraMode::FREE_FLY);
    camera->SetSmoothMovement(true, 8.0f);

    lightingSystem = std::make_unique<LightingSystem>();
    materialManager = std::make_unique<MaterialManager>();
    cityManager = std::make_unique<CityManager>(42);

    // Load shaders with proper error handling
    Shader advancedShader("shaders/advancedVertex.vs", "shaders/advancedFragment.fs");
    Shader shadowMapShader("shaders/shadowMap.vs", "shaders/shadowMap.fs");
    Shader hdrShader("shaders/postProcess.vs", "shaders/postProcess.fs");
    Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");

    setupFramebuffers();
    cityManager->generateComplexScene(3);

    // Setup lighting
    lightingSystem->addDirectionalLight(glm::vec3(-0.3f, -1.0f, -0.5f), glm::vec3(1.0f, 0.95f, 0.8f));

    std::vector<glm::vec3> streetLightPositions;
    for (int i = -40; i <= 40; i += 15) {
        for (int j = -40; j <= 40; j += 15) {
            streetLightPositions.push_back(glm::vec3(i, 0, j));
        }
    }
    lightingSystem->addStreetLights(streetLightPositions);

    // Create procedural skybox
    unsigned int cubemapTexture = createProceduralSkybox();

    // Cube vertices for basic geometry
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

    // Skybox vertices
    float skyboxVertices[] = {
        // positions          
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

    unsigned int VBO, skyboxVBO;

    // Setup cube VAO
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Setup skybox VAO
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Create materials
    Material* concreteMaterial = materialManager->createConcreteMaterial("concrete");
    Material* metalMaterial = materialManager->createMetalMaterial("metal", glm::vec3(0.7f, 0.7f, 0.8f));
    Material* glassMaterial = materialManager->createGlassMaterial("glass", glm::vec3(0.8f, 0.9f, 1.0f));
    Material* vegetationMaterial = materialManager->createVegetationMaterial("vegetation");

    std::cout << "Advanced 3D City Simulation Initialized" << std::endl;
    std::cout << "Scene Statistics:" << std::endl;
    auto sceneStats = cityManager->getSceneStatistics();
    std::cout << "  Total Objects: " << sceneStats.totalObjects << std::endl;
    std::cout << "  Hierarchy Levels: " << sceneStats.hierarchyLevels << std::endl;
    std::cout << "  Materials: " << materialManager->getMaterialCount() << std::endl;
    std::cout << "  Lights: " << lightingSystem->getLights().size() << std::endl;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        updatePerformanceStats();
        processInput(window);

        camera->Update(deltaTime);
        lightingSystem->update(deltaTime);
        cityManager->update(glm::mat4(1.0f));
        cityManager->optimizeScene(camera->GetPosition(),
            camera->GetProjectionMatrix(ASPECT_RATIO) * camera->GetViewMatrix());

        // Render to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Sky color based on day/night cycle
        glm::vec3 skyColor = lightingSystem->getSkyColor();
        glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);

        // Render skybox first
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 view = glm::mat4(glm::mat3(camera->GetViewMatrix()));
        glm::mat4 projection = camera->GetProjectionMatrix(ASPECT_RATIO);
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        // Pass day/night cycle info to skybox
        skyboxShader.setFloat("dayNightCycle", lightingSystem->getDayNightCycle());
        skyboxShader.setVec3("sunDirection", lightingSystem->getSunDirection());

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // Render main scene
        renderScene(advancedShader.ID, false);

        // Render UI if enabled
        if (showUI) {
            renderUI();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &skyboxVBO);
    if (hdrFBO) glDeleteFramebuffers(1, &hdrFBO);
    if (hdrColorBuffer) glDeleteTextures(1, &hdrColorBuffer);
    if (hdrDepthBuffer) glDeleteRenderbuffers(1, &hdrDepthBuffer);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (cubemapTexture) glDeleteTextures(1, &cubemapTexture);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::DOWN, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::ROLL_LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera->ProcessKeyboard(Camera_Movement::ROLL_RIGHT, deltaTime);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys[key] = true;
        keyPressed[key] = true;

        switch (key) {
        case GLFW_KEY_1:
            camera->SetMode(CameraMode::FREE_FLY);
            std::cout << "Camera Mode: Free Fly" << std::endl;
            break;
        case GLFW_KEY_2:
            camera->SetMode(CameraMode::ORBITAL);
            camera->SetTarget(glm::vec3(0, 0, 0));
            std::cout << "Camera Mode: Orbital" << std::endl;
            break;
        case GLFW_KEY_3:
            camera->SetMode(CameraMode::CINEMATIC);
            std::cout << "Camera Mode: Cinematic" << std::endl;
            break;
        case GLFW_KEY_H:
            enableHDR = !enableHDR;
            std::cout << "HDR: " << (enableHDR ? "ON" : "OFF") << std::endl;
            break;
        case GLFW_KEY_B:
            enableBloom = !enableBloom;
            std::cout << "Bloom: " << (enableBloom ? "ON" : "OFF") << std::endl;
            break;
        case GLFW_KEY_N:
            lightingSystem->setDayNightSpeed(lightingSystem->getDayNightSpeed() > 0.0f ? -0.1f : 0.1f);
            std::cout << "Day/Night cycle speed: " << lightingSystem->getDayNightSpeed() << std::endl;
            break;
        case GLFW_KEY_F:
            enableWireframe = !enableWireframe;
            glPolygonMode(GL_FRONT_AND_BACK, enableWireframe ? GL_LINE : GL_FILL);
            std::cout << "Wireframe: " << (enableWireframe ? "ON" : "OFF") << std::endl;
            break;
        case GLFW_KEY_U:
            showUI = !showUI;
            break;
        case GLFW_KEY_R:
            cityManager->generateComplexScene(3);
            std::cout << "City regenerated" << std::endl;
            break;
        }
    }
    else if (action == GLFW_RELEASE) {
        keys[key] = false;
        keyPressed[key] = false;
    }
}

void renderScene(unsigned int shaderProgram, bool shadowPass) {
    glUseProgram(shaderProgram);

    if (!shadowPass) {
        glm::mat4 projection = camera->GetProjectionMatrix(ASPECT_RATIO);
        glm::mat4 view = camera->GetViewMatrix();

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(camera->GetPosition()));

        lightingSystem->setupLightsForShader(shaderProgram);

        if (enableShadows) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, lightingSystem->getShadowMap());
        }
    }

    // Render ground plane
    Material* concreteMaterial = materialManager->getMaterial("concrete");
    if (concreteMaterial && !shadowPass) {
        concreteMaterial->bindToShader(shaderProgram);
        concreteMaterial->bindTextures();
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(200.0f, 0.1f, 200.0f));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Render roads
    if (!shadowPass) {
        Material* roadMaterial = materialManager->getMaterial("concrete");
        if (roadMaterial) {
            roadMaterial->bindToShader(shaderProgram);
            roadMaterial->bindTextures();
        }
    }

    for (const auto& road : cityManager->getRoads()) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(road.x, road.y, road.z));

        if (road.isHorizontal) {
            model = glm::scale(model, glm::vec3(road.length, 0.15f, road.width));
        }
        else {
            model = glm::scale(model, glm::vec3(road.width, 0.15f, road.length));
        }

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Render buildings
    for (const auto& building : cityManager->getVisibleBuildings()) {
        if (!shadowPass) {
            Material* buildingMaterial = materialManager->getMaterial("concrete");
            if (buildingMaterial) {
                buildingMaterial->bindToShader(shaderProgram);
                buildingMaterial->bindTextures();
            }
        }

        model = glm::mat4(1.0f);
        model = glm::translate(model, building->position + glm::vec3(0, building->scale.y / 2, 0));
        model = glm::scale(model, building->scale);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Render entities
    for (const auto& entity : cityManager->getVisibleEntities()) {
        if (!shadowPass) {
            Material* entityMaterial = nullptr;
            switch (entity->type) {
            case EntityType::TREE:
                entityMaterial = materialManager->getMaterial("vegetation");
                break;
            case EntityType::CAR:
                entityMaterial = materialManager->getMaterial("metal");
                break;
            default:
                entityMaterial = materialManager->getMaterial("concrete");
                break;
            }

            if (entityMaterial) {
                entityMaterial->bindToShader(shaderProgram);
                entityMaterial->bindTextures();
            }
        }

        if (entity->type == EntityType::HUMAN) {
            entity->renderHuman(shaderProgram, cubeVAO, 0, 0, 0);
        }
        else {
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(entity->worldTransform));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
}

void setupFramebuffers() {
    // HDR framebuffer
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    // HDR color buffer
    glGenTextures(1, &hdrColorBuffer);
    glBindTexture(GL_TEXTURE_2D, hdrColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorBuffer, 0);

    // HDR depth buffer
    glGenRenderbuffers(1, &hdrDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hdrDepthBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "HDR framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Setup post-processing quad
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void renderPostProcessing(Shader& hdrShader) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    hdrShader.use();
    hdrShader.setFloat("exposure", exposure);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColorBuffer);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_DEPTH_TEST);
}

void updatePerformanceStats() {
    perfStats.frameCount++;
    perfStats.fpsTimer += deltaTime;
    perfStats.frameTime = deltaTime * 1000.0f;

    if (perfStats.fpsTimer >= 1.0f) {
        perfStats.fps = perfStats.frameCount;
        perfStats.frameCount = 0;
        perfStats.fpsTimer = 0.0f;

        auto sceneStats = cityManager->getSceneStatistics();
        perfStats.visibleObjects = sceneStats.visibleObjects;
        perfStats.totalObjects = sceneStats.totalObjects;
    }
}

void renderUI() {
    static float uiTimer = 0.0f;
    uiTimer += deltaTime;

    if (uiTimer > 2.0f) {
        uiTimer = 0.0f;
        std::cout << "\n=== Performance Stats ===" << std::endl;
        std::cout << "FPS: " << perfStats.fps << std::endl;
        std::cout << "Frame Time: " << perfStats.frameTime << "ms" << std::endl;
        std::cout << "Visible Objects: " << perfStats.visibleObjects << "/" << perfStats.totalObjects << std::endl;
        std::cout << "Camera Mode: ";
        switch (camera->GetMode()) {
        case CameraMode::FREE_FLY: std::cout << "Free Fly"; break;
        case CameraMode::ORBITAL: std::cout << "Orbital"; break;
        case CameraMode::CINEMATIC: std::cout << "Cinematic"; break;
        default: std::cout << "Unknown"; break;
        }
        std::cout << std::endl;
        std::cout << "Day/Night Cycle: " << lightingSystem->getDayNightCycle() << std::endl;
        std::cout << "Sun Direction: " << lightingSystem->getSunDirection().x << ", " << lightingSystem->getSunDirection().y << ", " << lightingSystem->getSunDirection().z << std::endl;
        std::cout << "=========================" << std::endl;
    }
}

unsigned int createProceduralSkybox() {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    const int size = 512;

    // Create each face of the cubemap
    for (unsigned int i = 0; i < 6; i++) {
        std::vector<unsigned char> data(size * size * 3);

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int index = (y * size + x) * 3;

                // Map texture coordinates to 3D direction
                float u = (x / float(size)) * 2.0f - 1.0f;
                float v = (y / float(size)) * 2.0f - 1.0f;

                glm::vec3 dir;
                switch (i) {
                case 0: dir = glm::normalize(glm::vec3(1, -v, -u)); break;  // +X
                case 1: dir = glm::normalize(glm::vec3(-1, -v, u)); break;  // -X
                case 2: dir = glm::normalize(glm::vec3(u, 1, v)); break;    // +Y
                case 3: dir = glm::normalize(glm::vec3(u, -1, -v)); break;  // -Y
                case 4: dir = glm::normalize(glm::vec3(u, -v, 1)); break;   // +Z
                case 5: dir = glm::normalize(glm::vec3(-u, -v, -1)); break; // -Z
                }

                // Create gradient based on direction
                float height = dir.y;
                glm::vec3 color;

                if (height > 0.1f) {
                    // Sky - blue to white gradient
                    float t = glm::smoothstep(0.1f, 1.0f, height);
                    color = glm::mix(glm::vec3(0.5f, 0.7f, 1.0f), glm::vec3(0.8f, 0.9f, 1.0f), t);
                }
                else if (height > -0.1f) {
                    // Horizon - warm colors
                    color = glm::vec3(1.0f, 0.7f, 0.5f);
                }
                else {
                    // Ground reflection
                    color = glm::vec3(0.3f, 0.4f, 0.5f);
                }

                // Add some noise for texture
                float noise = (sin(dir.x * 20.0f) * cos(dir.z * 15.0f)) * 0.05f;
                color += noise;

                // Clamp colors
                color = glm::clamp(color, 0.0f, 1.0f);

                data[index] = (unsigned char)(color.r * 255);
                data[index + 1] = (unsigned char)(color.g * 255);
                data[index + 2] = (unsigned char)(color.b * 255);
            }
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB,
            size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    bool allLoaded = true;

    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            allLoaded = false;
        }
    }

    if (!allLoaded) {
        glDeleteTextures(1, &textureID);
        return createProceduralSkybox(); // Fallback to procedural skybox
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera->ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera->ProcessMouseScroll(yoffset);
}