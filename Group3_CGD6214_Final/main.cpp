#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include "shader.h"
#include "camera.h"
#include "CityManager.h"
#include "Entity.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Camera
Camera camera(glm::vec3(0.0f, 15.0f, 30.0f)); // 提高视角
float lastX = 1280.0f / 2.0;
float lastY = 720.0f / 2.0;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    // Fly up/down
    float cameraSpeed = 15.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.Position += camera.WorldUp * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.Position -= camera.WorldUp * cameraSpeed;
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
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(yoffset);
}

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = nrChannels == 3 ? GL_RGB : GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        std::cout << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// Helper function to get entity position from transform matrix
glm::vec3 getPositionFromTransform(const glm::mat4& transform) {
    return glm::vec3(transform[3]);
}

// Helper function to create a simple model matrix for non-human entities
glm::mat4 createEntityModelMatrix(const Entity& entity) {
    glm::vec3 position = getPositionFromTransform(entity.worldTransform);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);

    glm::vec3 scale(0.5f);

    switch (entity.type) {
    case EntityType::CAR:
        scale = glm::vec3(1.5f, 0.8f, 3.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.4f, 0.0f));
        break;
    case EntityType::TREE:
        scale = glm::vec3(1.2f, 2.5f, 1.2f);
        model = glm::translate(model, glm::vec3(0.0f, 1.25f, 0.0f));
        break;
    case EntityType::LAMP_POST:
        scale = glm::vec3(0.2f, 2.0f, 0.2f);
        model = glm::translate(model, glm::vec3(0.0f, 1.0f, 0.0f));
        break;
    case EntityType::TRASH_BIN:
        scale = glm::vec3(0.6f, 0.8f, 0.6f);
        model = glm::translate(model, glm::vec3(0.0f, 0.4f, 0.0f));
        break;
    case EntityType::GRASS_PATCH:
        scale = glm::vec3(4.0f, 0.1f, 4.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.05f, 0.0f));
        break;
    default:
        model = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0f));
        break;
    }

    model = glm::scale(model, scale);
    return model;
}

int main() {
    // GLFW init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "City Simulation with Wide Roads", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader ourShader("shaders/vertexShader.vs", "shaders/fragmentShader.fs");

    // Cube vertices
    float vertices[] = {
        -0.5f,-0.5f,-0.5f,  0.0f,0.0f,-1.0f,  0.0f,0.0f,
         0.5f,-0.5f,-0.5f,  0.0f,0.0f,-1.0f,  1.0f,0.0f,
         0.5f, 0.5f,-0.5f,  0.0f,0.0f,-1.0f,  1.0f,1.0f,
         0.5f, 0.5f,-0.5f,  0.0f,0.0f,-1.0f,  1.0f,1.0f,
        -0.5f, 0.5f,-0.5f,  0.0f,0.0f,-1.0f,  0.0f,1.0f,
        -0.5f,-0.5f,-0.5f,  0.0f,0.0f,-1.0f,  0.0f,0.0f,

        -0.5f,-0.5f, 0.5f,  0.0f,0.0f,1.0f,   0.0f,0.0f,
         0.5f,-0.5f, 0.5f,  0.0f,0.0f,1.0f,   1.0f,0.0f,
         0.5f, 0.5f, 0.5f,  0.0f,0.0f,1.0f,   1.0f,1.0f,
         0.5f, 0.5f, 0.5f,  0.0f,0.0f,1.0f,   1.0f,1.0f,
        -0.5f, 0.5f, 0.5f,  0.0f,0.0f,1.0f,   0.0f,1.0f,
        -0.5f,-0.5f, 0.5f,  0.0f,0.0f,1.0f,   0.0f,0.0f,

        -0.5f, 0.5f, 0.5f, -1.0f,0.0f,0.0f,   1.0f,0.0f,
        -0.5f, 0.5f,-0.5f, -1.0f,0.0f,0.0f,   1.0f,1.0f,
        -0.5f,-0.5f,-0.5f, -1.0f,0.0f,0.0f,   0.0f,1.0f,
        -0.5f,-0.5f,-0.5f, -1.0f,0.0f,0.0f,   0.0f,1.0f,
        -0.5f,-0.5f, 0.5f, -1.0f,0.0f,0.0f,   0.0f,0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f,0.0f,0.0f,   1.0f,0.0f,

         0.5f, 0.5f, 0.5f,  1.0f,0.0f,0.0f,   1.0f,0.0f,
         0.5f, 0.5f,-0.5f,  1.0f,0.0f,0.0f,   1.0f,1.0f,
         0.5f,-0.5f,-0.5f,  1.0f,0.0f,0.0f,   0.0f,1.0f,
         0.5f,-0.5f,-0.5f,  1.0f,0.0f,0.0f,   0.0f,1.0f,
         0.5f,-0.5f, 0.5f,  1.0f,0.0f,0.0f,   0.0f,0.0f,
         0.5f, 0.5f, 0.5f,  1.0f,0.0f,0.0f,   1.0f,0.0f,

        -0.5f,-0.5f,-0.5f,  0.0f,-1.0f,0.0f,  0.0f,1.0f,
         0.5f,-0.5f,-0.5f,  0.0f,-1.0f,0.0f,  1.0f,1.0f,
         0.5f,-0.5f, 0.5f,  0.0f,-1.0f,0.0f,  1.0f,0.0f,
         0.5f,-0.5f, 0.5f,  0.0f,-1.0f,0.0f,  1.0f,0.0f,
        -0.5f,-0.5f, 0.5f,  0.0f,-1.0f,0.0f,  0.0f,0.0f,
        -0.5f,-0.5f,-0.5f,  0.0f,-1.0f,0.0f,  0.0f,1.0f,

        -0.5f, 0.5f,-0.5f,  0.0f,1.0f,0.0f,   0.0f,1.0f,
         0.5f, 0.5f,-0.5f,  0.0f,1.0f,0.0f,   1.0f,1.0f,
         0.5f, 0.5f, 0.5f,  0.0f,1.0f,0.0f,   1.0f,0.0f,
         0.5f, 0.5f, 0.5f,  0.0f,1.0f,0.0f,   1.0f,0.0f,
        -0.5f, 0.5f, 0.5f,  0.0f,1.0f,0.0f,   0.0f,0.0f,
        -0.5f, 0.5f,-0.5f,  0.0f,1.0f,0.0f,   0.0f,1.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Load textures
    unsigned int texBuilding = loadTexture("textures/building.jpg");
    unsigned int texRoad = loadTexture("textures/road.jpg");
    unsigned int texCar = loadTexture("textures/car.jpg");
    unsigned int texTree = loadTexture("textures/tree.jpg");
    unsigned int texHumanWhite = loadTexture("textures/human_white.jpg");
    unsigned int texHumanBlack = loadTexture("textures/human_black.jpg");
    unsigned int texHumanYellow = loadTexture("textures/human_yellow.jpg");
    unsigned int texGrass = loadTexture("textures/grass.jpg");
    unsigned int texLamp = loadTexture("textures/lamp.jpg");
    unsigned int texTrash = loadTexture("textures/trash.jpg");

    ourShader.use();
    ourShader.setInt("texture1", 0);

    // Init city with larger grid
    CityManager cityManager;
    cityManager.generateCity(12, 12.0f); // 更大的网格
    cityManager.spawnEntities(15, 8, 10, 8); // 更多实体

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        glClearColor(0.6f, 0.8f, 1.0f, 1.0f); // 更亮的天空色
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 1280.0f / 720.0f, 0.1f, 500.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        // Update entities
        cityManager.update(deltaTime);

        // 先渲染地面
        glm::mat4 groundModel = glm::mat4(1.0f);
        groundModel = glm::translate(groundModel, glm::vec3(50.0f, -0.1f, 50.0f));
        groundModel = glm::scale(groundModel, glm::vec3(200.0f, 0.1f, 200.0f));
        ourShader.setMat4("model", groundModel);
        glBindTexture(GL_TEXTURE_2D, texGrass);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Render roads
        for (const auto& r : cityManager.getRoads()) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(r.x, r.y, r.z));

            if (r.isHorizontal) {
                // 横向道路
                model = glm::scale(model, glm::vec3(r.length, 0.15f, r.width));
            }
            else {
                // 纵向道路
                model = glm::scale(model, glm::vec3(r.width, 0.15f, r.length));
            }

            ourShader.setMat4("model", model);
            glBindTexture(GL_TEXTURE_2D, texRoad);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Render buildings
        for (const auto& b : cityManager.getBuildings()) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(b.x, b.height / 2.0f, b.z));
            model = glm::scale(model, glm::vec3(b.width, b.height, b.depth));
            ourShader.setMat4("model", model);
            glBindTexture(GL_TEXTURE_2D, texBuilding);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Render entities
        for (const auto& entity : cityManager.getEntities()) {
            if (entity.type == EntityType::HUMAN) {
                entity.renderHuman(ourShader.ID, VAO, texHumanWhite, texHumanBlack, texHumanYellow);
            }
            else {
                glm::mat4 model = createEntityModelMatrix(entity);
                ourShader.setMat4("model", model);

                switch (entity.type) {
                case EntityType::CAR:
                    glBindTexture(GL_TEXTURE_2D, texCar);
                    break;
                case EntityType::TREE:
                    glBindTexture(GL_TEXTURE_2D, texTree);
                    break;
                case EntityType::LAMP_POST:
                    glBindTexture(GL_TEXTURE_2D, texLamp);
                    break;
                case EntityType::TRASH_BIN:
                    glBindTexture(GL_TEXTURE_2D, texTrash);
                    break;
                case EntityType::GRASS_PATCH:
                    glBindTexture(GL_TEXTURE_2D, texGrass);
                    break;
                default:
                    glBindTexture(GL_TEXTURE_2D, texHumanWhite);
                    break;
                }

                glBindVertexArray(VAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glfwTerminate();
    return 0;
}