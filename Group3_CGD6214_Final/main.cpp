#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include "shader.h"
#include "camera.h"
#include "Node.h"
#include "Building.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Camera
Camera camera(glm::vec3(0.0f, 3.0f, 15.0f));
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

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Planned City GTA-V Style", NULL, NULL);
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

    float vertices[] = {
        // positions          // normals       // texcoords
        -0.5f,-0.5f,-0.5f, 0,0,-1, 0.0f,0.0f,
         0.5f,-0.5f,-0.5f, 0,0,-1, 1.0f,0.0f,
         0.5f, 0.5f,-0.5f, 0,0,-1, 1.0f,1.0f,
         0.5f, 0.5f,-0.5f, 0,0,-1, 1.0f,1.0f,
        -0.5f, 0.5f,-0.5f, 0,0,-1, 0.0f,1.0f,
        -0.5f,-0.5f,-0.5f, 0,0,-1, 0.0f,0.0f,

        -0.5f,-0.5f, 0.5f, 0,0,1, 0.0f,0.0f,
         0.5f,-0.5f, 0.5f, 0,0,1, 1.0f,0.0f,
         0.5f, 0.5f, 0.5f, 0,0,1, 1.0f,1.0f,
         0.5f, 0.5f, 0.5f, 0,0,1, 1.0f,1.0f,
        -0.5f, 0.5f, 0.5f, 0,0,1, 0.0f,1.0f,
        -0.5f,-0.5f, 0.5f, 0,0,1, 0.0f,0.0f,

        -0.5f, 0.5f, 0.5f,-1,0,0, 0.0f,0.0f,
        -0.5f, 0.5f,-0.5f,-1,0,0, 1.0f,0.0f,
        -0.5f,-0.5f,-0.5f,-1,0,0, 1.0f,1.0f,
        -0.5f,-0.5f,-0.5f,-1,0,0, 1.0f,1.0f,
        -0.5f,-0.5f, 0.5f,-1,0,0, 0.0f,1.0f,
        -0.5f, 0.5f, 0.5f,-1,0,0, 0.0f,0.0f,

         0.5f, 0.5f, 0.5f,1,0,0, 1.0f,0.0f,
         0.5f, 0.5f,-0.5f,1,0,0, 0.0f,0.0f,
         0.5f,-0.5f,-0.5f,1,0,0, 0.0f,1.0f,
         0.5f,-0.5f,-0.5f,1,0,0, 0.0f,1.0f,
         0.5f,-0.5f, 0.5f,1,0,0, 1.0f,1.0f,
         0.5f, 0.5f, 0.5f,1,0,0, 1.0f,0.0f,

        -0.5f,-0.5f,-0.5f,0,-1,0, 0.0f,1.0f,
         0.5f,-0.5f,-0.5f,0,-1,0, 1.0f,1.0f,
         0.5f,-0.5f, 0.5f,0,-1,0, 1.0f,0.0f,
         0.5f,-0.5f, 0.5f,0,-1,0, 1.0f,0.0f,
        -0.5f,-0.5f, 0.5f,0,-1,0, 0.0f,0.0f,
        -0.5f,-0.5f,-0.5f,0,-1,0, 0.0f,1.0f,

        -0.5f, 0.5f,-0.5f,0,1,0, 0.0f,1.0f,
         0.5f, 0.5f,-0.5f,0,1,0, 1.0f,1.0f,
         0.5f, 0.5f, 0.5f,0,1,0, 1.0f,0.0f,
         0.5f, 0.5f, 0.5f,0,1,0, 1.0f,0.0f,
        -0.5f, 0.5f, 0.5f,0,1,0, 0.0f,0.0f,
        -0.5f, 0.5f,-0.5f,0,1,0, 0.0f,1.0f
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

    unsigned int texGrass = loadTexture("textures/grass.jpg");
    unsigned int texRoad = loadTexture("textures/road.jpg");
    unsigned int texHigh = loadTexture("textures/high.jpg");

    ourShader.use();
    ourShader.setInt("texture1", 0);

    Node cityRoot;

    Building ground(glm::vec3(0, -1, 0), glm::vec3(50, 0.2, 50), BuildingType::FIELD);
    Building road(glm::vec3(0, -0.9, 0), glm::vec3(40, 0.1, 6), BuildingType::ROAD);
    Building skyscraper(glm::vec3(5, 0, -5), glm::vec3(4, 20, 4), BuildingType::SKYSCRAPER);

    cityRoot.addChild(&ground);
    cityRoot.addChild(&road);
    cityRoot.addChild(&skyscraper);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 1280.0f / 720.0f, 0.1f, 200.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        cityRoot.update();
        for (Node* child : cityRoot.children) {
            Building* building = dynamic_cast<Building*>(child);
            if (!building) continue;
            glm::mat4 model = building->worldTransform;
            ourShader.setMat4("model", model);

            if (building->type == BuildingType::FIELD)
                glBindTexture(GL_TEXTURE_2D, texGrass);
            else if (building->type == BuildingType::ROAD)
                glBindTexture(GL_TEXTURE_2D, texRoad);
            else
                glBindTexture(GL_TEXTURE_2D, texHigh);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glfwTerminate();
    return 0;
}
