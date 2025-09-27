#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "Camera.h"
#include "Shader.h"

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

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
GLuint createCube();
GLuint createGround();

int main()
{
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Interactive 3D City", NULL, NULL);
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
    if (!buildingShader.LoadFromFile("shaders/basic.vert", "shaders/lighting.frag" )) {
        std::cout << "Failed to load shader files!" << std::endl;
        std::cout << "Make sure you have:" << std::endl;
        std::cout << "  - shaders/basic.vert" << std::endl;
        std::cout << "  - shaders/Lighting.frag" << std::endl;
        std::cout << "in your project directory" << std::endl;
        return -1;
    }

    std::cout << "Successfully loaded shaders from files!" << std::endl;

    // Create geometry
    GLuint cubeVAO = createCube();
    GLuint groundVAO = createGround();

    // Set up camera boundaries for the city
    camera.SetBoundaries(-50.0f, 50.0f, -50.0f, 50.0f, 1.5f, 30.0f);
    camera.SetCollisionRadius(1.5f);
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

        // Update camera
        camera.UpdateSmoothMovement(deltaTime);
        camera.UpdateOrbitalCamera(deltaTime);
        camera.UpdateTransition(deltaTime);

        // Render
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        buildingShader.Use();

        // View/projection transformations using camera
        glm::mat4 projection = camera.GetProjectionMatrix((float)WIDTH / (float)HEIGHT);
        glm::mat4 view = camera.GetViewMatrix();
        buildingShader.SetMat4("projection", projection);
        buildingShader.SetMat4("view", view);

        // Set lighting uniforms
        buildingShader.SetVec3("lightPos", glm::vec3(10.0f, 15.0f, 10.0f));
        buildingShader.SetVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        buildingShader.SetVec3("viewPos", camera.GetPosition());

        // Render ground
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(100.0f, 1.0f, 100.0f));
        buildingShader.SetMat4("model", model);
        buildingShader.SetVec3("objectColor", glm::vec3(0.3f, 0.3f, 0.3f));

        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Render buildings with variety
        glBindVertexArray(cubeVAO);

        // Building colors array
        glm::vec3 buildingColors[] = {
            glm::vec3(0.6f, 0.6f, 0.8f),  // Blue-gray
            glm::vec3(0.8f, 0.7f, 0.6f),  // Tan/beige
            glm::vec3(0.7f, 0.5f, 0.5f),  // Red-brown
            glm::vec3(0.5f, 0.7f, 0.5f),  // Green
            glm::vec3(0.8f, 0.6f, 0.4f),  // Orange-brown
            glm::vec3(0.6f, 0.5f, 0.8f),  // Purple-gray
            glm::vec3(0.7f, 0.7f, 0.5f),  // Yellow-gray
            glm::vec3(0.5f, 0.6f, 0.7f)   // Steel blue
        };

        // Roof color (darker than buildings)
        glm::vec3 roofColor = glm::vec3(0.3f, 0.2f, 0.2f);  // Dark brown

        // Seed random for consistent building generation
        srand(12345);  // Fixed seed for consistent results

        // Create city blocks with streets
        for (int x = -40; x <= 40; x += 12)
        {
            for (int z = -40; z <= 40; z += 12)
            {
                // Skip center area and create streets
                if ((x >= -6 && x <= 6) || (z >= -6 && z <= 6)) continue;  // Main streets
                if (x % 24 == 0 || z % 24 == 0) continue;  // Secondary streets

                // Random building properties
                float width = 1.5f + (rand() % 3) * 0.5f;     // Width: 1.5 to 3.0
                float depth = 1.5f + (rand() % 3) * 0.5f;     // Depth: 1.5 to 3.0
                float height = 3.0f + (rand() % 8) * 1.5f;    // Height: 3.0 to 13.5
                bool hasRoof = (rand() % 3) == 0;              // 33% chance of roof
                int colorIndex = rand() % 8;                   // Random color

                // Render main building
                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(x, height / 2.0f, z));
                model = glm::scale(model, glm::vec3(width, height, depth));
                buildingShader.SetMat4("model", model);

                buildingShader.SetVec3("objectColor", buildingColors[colorIndex]);

                glDrawArrays(GL_TRIANGLES, 0, 36);

                // Render roof if building has one
                if (hasRoof)
                {
                    float roofHeight = 0.8f;
                    float roofWidth = width + 0.3f;   // Slightly wider than building
                    float roofDepth = depth + 0.3f;   // Slightly deeper than building

                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(x, height + roofHeight / 2.0f, z));
                    model = glm::scale(model, glm::vec3(roofWidth, roofHeight, roofDepth));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", roofColor);

                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }

                // Add some variety with additional building elements
                // Small buildings sometimes get antenna or chimneys
                if (height < 6.0f && (rand() % 4) == 0)  // 25% chance for small buildings
                {
                    // Add antenna/chimney
                    float antennaHeight = 1.0f + (rand() % 2) * 0.5f;
                    float antennaSize = 0.1f + (rand() % 2) * 0.05f;

                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(x, height + antennaHeight / 2.0f, z));
                    model = glm::scale(model, glm::vec3(antennaSize, antennaHeight, antennaSize));
                    buildingShader.SetMat4("model", model);
                    buildingShader.SetVec3("objectColor", glm::vec3(0.4f, 0.4f, 0.4f));

                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }

                // Tall buildings sometimes get additional sections
                if (height > 10.0f && (rand() % 3) == 0)  // 33% chance for tall buildings
                {
                    // Add upper section with different size
                    float upperWidth = width * 0.7f;
                    float upperDepth = depth * 0.7f;
                    float upperHeight = height * 0.3f;

                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(x, height + upperHeight / 2.0f, z));
                    model = glm::scale(model, glm::vec3(upperWidth, upperHeight, upperDepth));
                    buildingShader.SetMat4("model", model);

                    // Slightly different color for upper section
                    glm::vec3 upperColor = buildingColors[colorIndex] * 0.8f;  // Darker
                    buildingShader.SetVec3("objectColor", upperColor);

                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            }
        }

        // Add some scattered smaller buildings in empty areas
        srand(54321);  // Different seed for scattered buildings
        for (int i = 0; i < 15; i++)  // Add 15 random small buildings
        {
            float x = -35.0f + (rand() / float(RAND_MAX)) * 70.0f;  // Random X position
            float z = -35.0f + (rand() / float(RAND_MAX)) * 70.0f;  // Random Z position

            // Avoid main streets and center
            if ((x >= -8 && x <= 8) || (z >= -8 && z <= 8)) continue;

            float width = 1.0f + (rand() % 2) * 0.5f;      // Small width: 1.0 to 1.5
            float depth = 1.0f + (rand() % 2) * 0.5f;      // Small depth: 1.0 to 1.5
            float height = 1.5f + (rand() % 4) * 0.8f;     // Small height: 1.5 to 3.9
            int colorIndex = rand() % 8;

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, height / 2.0f, z));
            model = glm::scale(model, glm::vec3(width, height, depth));
            buildingShader.SetMat4("model", model);
            buildingShader.SetVec3("objectColor", buildingColors[colorIndex]);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &groundVAO);

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
            camera.SetOrbitTarget(glm::vec3(0.0f, 5.0f, 0.0f), 25.0f);
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

    // Hot reload shaders (F5) - This will now work with files!
    static bool f5KeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS && !f5KeyPressed) {
        f5KeyPressed = true;
        std::cout << "Reloading shaders from files..." << std::endl;

        // Get the current shader and reload it
        // You'll need a global reference to your shader for this to work
        // For now, just print a message
        std::cout << "Shader hot-reload feature requires global shader reference" << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_RELEASE) {
        f5KeyPressed = false;
    }

    // Speed adjustment
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera.MovementSpeed = 20.0f;  // Fast movement
    }
    else {
        camera.MovementSpeed = 10.0f;  // Normal speed
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

GLuint createGround()
{
    // Ground vertices (a large plane)
    float vertices[] = {
        // positions          // normals           // texture coords
        -1.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         1.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
         1.0f, 0.0f,  1.0f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        -1.0f, 0.0f,  1.0f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
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