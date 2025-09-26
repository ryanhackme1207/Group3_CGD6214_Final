// LightingSystem.cpp
#include "LightingSystem.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <string>

// LightingSystem.cpp Implementation
LightingSystem::LightingSystem()
    : dayNightCycle(0.5f), dayNightSpeed(0.1f), ambientStrength(0.2f), enableShadows(true),
    sunDirection(0.0f, -1.0f, 0.3f), sunColor(1.0f, 0.9f, 0.7f), moonColor(0.3f, 0.3f, 0.5f),
    skyColor(0.6f, 0.8f, 1.0f), shadowMapFBO(0), shadowMap(0) {

    // Setup shadow mapping
    setupShadowMapping();

    // Add default sun light
    addDirectionalLight(sunDirection, sunColor);
}

LightingSystem::~LightingSystem() {
    if (shadowMapFBO) {
        glDeleteFramebuffers(1, &shadowMapFBO);
        glDeleteTextures(1, &shadowMap);
    }
}

void LightingSystem::addDirectionalLight(const glm::vec3& direction, const glm::vec3& color) {
    auto light = std::make_unique<Light>(LightType::DIRECTIONAL);
    light->direction = glm::normalize(direction);
    light->color = color;
    light->diffuse = color * 0.8f;
    light->ambient = color * 0.1f;
    light->specular = color;
    lights.push_back(std::move(light));
}

void LightingSystem::addPointLight(const glm::vec3& position, const glm::vec3& color, float intensity) {
    auto light = std::make_unique<Light>(LightType::POINT);
    light->position = position;
    light->color = color;
    light->intensity = intensity;
    light->diffuse = color * 0.8f;
    light->ambient = color * 0.1f;
    light->specular = color;

    // Set attenuation based on desired range
    float range = 50.0f * intensity;
    light->constant = 1.0f;
    light->linear = 4.5f / range;
    light->quadratic = 75.0f / (range * range);

    lights.push_back(std::move(light));
}

void LightingSystem::addSpotLight(const glm::vec3& position, const glm::vec3& direction,
    const glm::vec3& color, float cutOff, float outerCutOff) {
    auto light = std::make_unique<Light>(LightType::SPOT);
    light->position = position;
    light->direction = glm::normalize(direction);
    light->color = color;
    light->cutOff = cutOff;
    light->outerCutOff = outerCutOff;
    light->diffuse = color * 0.8f;
    light->ambient = color * 0.1f;
    light->specular = color;

    // Spotlight attenuation
    light->constant = 1.0f;
    light->linear = 0.09f;
    light->quadratic = 0.032f;

    lights.push_back(std::move(light));
}

void LightingSystem::addStreetLights(const std::vector<glm::vec3>& positions) {
    for (const auto& pos : positions) {
        // Add warm street light
        addPointLight(pos + glm::vec3(0, 3.0f, 0), glm::vec3(1.0f, 0.8f, 0.6f), 0.8f);

        // Add spotlight pointing down
        addSpotLight(pos + glm::vec3(0, 3.0f, 0), glm::vec3(0, -1, 0),
            glm::vec3(1.0f, 0.8f, 0.6f), 25.0f, 35.0f);
    }
}

void LightingSystem::updateDayNightCycle(float deltaTime) {
    dayNightCycle += dayNightSpeed * deltaTime;
    if (dayNightCycle > 1.0f) dayNightCycle -= 1.0f;
    if (dayNightCycle < 0.0f) dayNightCycle += 1.0f;

    // Calculate sun position and color based on day/night cycle
    float angle = dayNightCycle * 2.0f * static_cast<float>(M_PI);
    sunDirection = glm::vec3(sin(angle), -cos(angle), 0.3f);

    // Determine if it's day or night
    bool isDay = sunDirection.y > -0.1f;

    if (isDay) {
        // Day colors
        float dayIntensity = std::max(0.1f, sunDirection.y);
        sunColor = glm::vec3(1.0f, 0.95f, 0.8f) * dayIntensity;
        skyColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.2f), glm::vec3(0.6f, 0.8f, 1.0f), dayIntensity);
        ambientStrength = 0.3f * dayIntensity + 0.1f;
    }
    else {
        // Night colors
        float nightIntensity = std::max(0.05f, -sunDirection.y * 0.3f);
        sunColor = moonColor * nightIntensity;
        skyColor = glm::vec3(0.05f, 0.05f, 0.2f);
        ambientStrength = 0.05f;
    }

    // Update directional light (sun/moon)
    if (!lights.empty() && lights[0]->type == LightType::DIRECTIONAL) {
        lights[0]->direction = sunDirection;
        lights[0]->color = sunColor;
        lights[0]->diffuse = sunColor * 0.8f;
        lights[0]->ambient = sunColor * ambientStrength;
    }
}

void LightingSystem::update(float deltaTime) {
    updateDayNightCycle(deltaTime);

    // Update animated lights
    for (auto& light : lights) {
        if (light->animated) {
            light->animationTime += deltaTime * light->animationSpeed;

            // Example: flickering street lights
            if (light->type == LightType::POINT) {
                float flicker = 0.8f + 0.2f * sin(light->animationTime * 10.0f) *
                    cos(light->animationTime * 7.0f);
                light->intensity = flicker;
            }
        }
    }
}

void LightingSystem::setupLightsForShader(unsigned int shaderProgram) const {
    glUseProgram(shaderProgram);

    // Set number of lights
    int numLights = std::min(static_cast<int>(lights.size()), 8); // Max 8 lights
    glUniform1i(glGetUniformLocation(shaderProgram, "numLights"), numLights);

    // Set individual light properties
    for (int i = 0; i < numLights; ++i) {
        std::string base = "lights[" + std::to_string(i) + "]";
        const auto& light = lights[i];

        glUniform1i(glGetUniformLocation(shaderProgram, (base + ".type").c_str()),
            static_cast<int>(light->type));
        glUniform3fv(glGetUniformLocation(shaderProgram, (base + ".position").c_str()),
            1, glm::value_ptr(light->position));
        glUniform3fv(glGetUniformLocation(shaderProgram, (base + ".direction").c_str()),
            1, glm::value_ptr(light->direction));
        glUniform3fv(glGetUniformLocation(shaderProgram, (base + ".color").c_str()),
            1, glm::value_ptr(light->color));
        glUniform3fv(glGetUniformLocation(shaderProgram, (base + ".ambient").c_str()),
            1, glm::value_ptr(light->ambient));
        glUniform3fv(glGetUniformLocation(shaderProgram, (base + ".diffuse").c_str()),
            1, glm::value_ptr(light->diffuse));
        glUniform3fv(glGetUniformLocation(shaderProgram, (base + ".specular").c_str()),
            1, glm::value_ptr(light->specular));
        glUniform1f(glGetUniformLocation(shaderProgram, (base + ".constant").c_str()),
            light->constant);
        glUniform1f(glGetUniformLocation(shaderProgram, (base + ".linear").c_str()),
            light->linear);
        glUniform1f(glGetUniformLocation(shaderProgram, (base + ".quadratic").c_str()),
            light->quadratic);
        glUniform1f(glGetUniformLocation(shaderProgram, (base + ".cutOff").c_str()),
            cos(glm::radians(light->cutOff)));
        glUniform1f(glGetUniformLocation(shaderProgram, (base + ".outerCutOff").c_str()),
            cos(glm::radians(light->outerCutOff)));
        glUniform1f(glGetUniformLocation(shaderProgram, (base + ".intensity").c_str()),
            light->intensity);
    }

    // Set global ambient
    glm::vec3 globalAmbient = skyColor * ambientStrength;
    glUniform3fv(glGetUniformLocation(shaderProgram, "globalAmbient"), 1, glm::value_ptr(globalAmbient));

    // Set shadow mapping uniforms
    if (enableShadows) {
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightSpaceMatrix"),
            1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniform1i(glGetUniformLocation(shaderProgram, "shadowMap"), 1); // Texture unit 1
    }
    glUniform1i(glGetUniformLocation(shaderProgram, "enableShadows"), enableShadows);
}

void LightingSystem::setupShadowMapping() {
    // Create shadow map framebuffer
    glGenFramebuffers(1, &shadowMapFBO);
    glGenTextures(1, &shadowMap);

    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT,
        0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void LightingSystem::renderShadowMap(unsigned int shadowShader, const std::function<void()>& renderScene) {
    if (!enableShadows || lights.empty()) return;

    // Calculate light space matrix for the main directional light
    glm::mat4 lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 1.0f, 100.0f);
    glm::mat4 lightView = glm::lookAt(-lights[0]->direction * 50.0f,
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    lightSpaceMatrix = lightProjection * lightView;

    // Render shadow map
    glUseProgram(shadowShader);
    glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"),
        1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT); // Prevent shadow acne

    renderScene(); // Render all shadow-casting objects

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}