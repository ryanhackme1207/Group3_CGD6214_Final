#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include "Shader.h"

class DeferredRenderer {
public:
    DeferredRenderer();
    ~DeferredRenderer();

    // Initialize G-Buffer and screen quad
    bool Initialize(int screenWidth, int screenHeight);
    
    // Cleanup resources
    void Cleanup();

    // Begin geometry pass - bind G-Buffer for writing
    void BeginGeometryPass();
    
    // End geometry pass
    void EndGeometryPass();
    
    // Begin lighting pass - render to screen using G-Buffer data
    void BeginLightingPass();
    
    // End lighting pass
    void EndLightingPass();
    
    // Render a fullscreen quad for lighting pass
    void RenderQuad();
    
    // Resize buffers when window size changes
    void Resize(int newWidth, int newHeight);
    
    // Get shader for geometry pass
    Shader& GetGeometryShader() { return *geometryShader; }
    
    // Get shader for lighting pass
    Shader& GetLightingShader() { return *lightingShader; }
    
    // Debug function to visualize G-Buffer contents
    void DebugDrawGBuffer(int bufferIndex = 0);

    // HDR settings
    void SetExposure(float value) { exposure = value; }
    float GetExposure() const { return exposure; }

    // Bloom controls
    void SetBloomEnabled(bool v) { bloomEnabled = v; }
    bool IsBloomEnabled() const { return bloomEnabled; }
    void SetBloomIntensity(float v) { bloomIntensity = v; }
    float GetBloomIntensity() const { return bloomIntensity; }

private:
    // Screen dimensions
    int screenWidth, screenHeight;
    
    // G-Buffer framebuffer and textures
    GLuint gBuffer;
    GLuint gPosition;    // RGB: world position, A: unused
    GLuint gNormal;      // RGB: world normal, A: unused  
    GLuint gAlbedoSpec;  // RGB: albedo (diffuse color), A: specular intensity
    GLuint gDepth;       // Depth buffer
    
    // HDR framebuffer objects
    GLuint hdrFBO;
    GLuint hdrColorBuffer;
    GLuint hdrRBO;
    
    // Bloom ping-pong framebuffers/textures
    GLuint pingpongFBO[2];
    GLuint pingpongColor[2];

    // Settings
    float exposure;
    bool bloomEnabled;
    float bloomIntensity;
    
    // Fullscreen quad for lighting pass
    GLuint quadVAO, quadVBO;
    
    // Shaders
    std::unique_ptr<Shader> geometryShader;
    std::unique_ptr<Shader> lightingShader;
    std::unique_ptr<Shader> debugShader;
    std::unique_ptr<Shader> hdrShader;          // tone mapping combine
    std::unique_ptr<Shader> bloomExtractShader; // bright pass
    std::unique_ptr<Shader> bloomBlurShader;    // gaussian blur
    
    // Initialize G-Buffer textures and framebuffer
    bool InitializeGBuffer();
    
    // Initialize HDR framebuffer
    bool InitializeHDRBuffer();
    
    // Initialize Bloom buffers
    bool InitializeBloomBuffers();
    
    // Initialize fullscreen quad
    void InitializeQuad();
    
    // Initialize shaders
    bool InitializeShaders();
};