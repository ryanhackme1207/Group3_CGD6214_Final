#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>
#include <functional>

enum class LightType {
    DIRECTIONAL = 0,
    POINT = 1,
    SPOT = 2
};

struct Light {
    LightType type;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    glm::vec3 ambient = glm::vec3(0.1f);
    glm::vec3 diffuse = glm::vec3(0.8f);
    glm::vec3 specular = glm::vec3(1.0f);

    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    float cutOff = 12.5f;
    float outerCutOff = 17.5f;
    float intensity = 1.0f;

    bool animated = false;
    float animationSpeed = 1.0f;
    float animationTime = 0.0f;

    Light(LightType t) : type(t) {}
};

class LightingSystem {
public:
    LightingSystem();
    ~LightingSystem();

    // Light management
    void addDirectionalLight(const glm::vec3& direction, const glm::vec3& color);
    void addPointLight(const glm::vec3& position, const glm::vec3& color, float intensity = 1.0f);
    void addSpotLight(const glm::vec3& position, const glm::vec3& direction,
        const glm::vec3& color, float cutOff = 12.5f, float outerCutOff = 17.5f);
    void addStreetLights(const std::vector<glm::vec3>& positions);

    // Day/night cycle
    void updateDayNightCycle(float deltaTime);
    void setDayNightSpeed(float speed) { dayNightSpeed = speed; }
    float getDayNightCycle() const { return dayNightCycle; }
    float getDayNightSpeed() const { return dayNightSpeed; }
    glm::vec3 getSkyColor() const { return skyColor; }
    glm::vec3 getSunDirection() const { return sunDirection; }

    // Update system
    void update(float deltaTime);

    // Shader setup
    void setupLightsForShader(unsigned int shaderProgram) const;

    // Shadow mapping
    void setupShadowMapping();
    void renderShadowMap(unsigned int shadowShader, const std::function<void()>& renderScene);
    unsigned int getShadowMap() const { return shadowMap; }

    // Getters
    const std::vector<std::unique_ptr<Light>>& getLights() const { return lights; }

private:
    std::vector<std::unique_ptr<Light>> lights;

    // Day/night cycle
    float dayNightCycle;
    float dayNightSpeed;
    float ambientStrength;
    glm::vec3 sunDirection;
    glm::vec3 sunColor;
    glm::vec3 moonColor;
    glm::vec3 skyColor;

    // Shadow mapping
    bool enableShadows;
    unsigned int shadowMapFBO;
    unsigned int shadowMap;
    glm::mat4 lightSpaceMatrix;
    static const int SHADOW_WIDTH = 1024;
    static const int SHADOW_HEIGHT = 1024;
};