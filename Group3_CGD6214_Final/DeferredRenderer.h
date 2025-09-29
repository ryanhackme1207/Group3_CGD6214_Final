#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
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

    // MSAA controls (for deferred path). samples=0 disables.
    void SetMSAASamples(int samples);
    int  GetMSAASamples() const { return msaaSamples; }

    // SSAO controls
    void SetSSAOEnabled(bool v){ ssaoEnabled = v; }
    bool IsSSAOEnabled() const { return ssaoEnabled; }
    void SetSSAORadius(float r){ ssaoRadius = r; }
    float GetSSAORadius() const { return ssaoRadius; }
    void SetSSAOBias(float b){ ssaoBias = b; }
    float GetSSAOBias() const { return ssaoBias; }

private:
    // Screen dimensions
    int screenWidth, screenHeight;

    // Single-sample G-buffer (used for lighting pass sampling)
    GLuint gBuffer; 
    GLuint gPosition; 
    GLuint gNormal; 
    GLuint gAlbedoSpec; 
    GLuint gDepth;

    // Multi-sample G-buffer (write target when msaaSamples>0) - resolved into single-sample above
    GLuint gBufferMS; 
    GLuint gPositionMS; 
    GLuint gNormalMS; 
    GLuint gAlbedoSpecMS; 
    GLuint gDepthMS; 
    int msaaSamples;

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
    
    // SSAO resources
    bool ssaoEnabled = true;
    float ssaoRadius = 0.9f;
    float ssaoBias = 0.025f;
    GLuint ssaoFBO = 0, ssaoColorBuffer = 0;
    GLuint ssaoBlurFBO = 0, ssaoColorBufferBlur = 0;
    GLuint noiseTexture = 0;
    std::unique_ptr<Shader> ssaoShader; 
    std::unique_ptr<Shader> ssaoBlurShader; 
    std::vector<glm::vec3> ssaoKernel; 

    // Initialize G-Buffer textures and framebuffer
    bool InitializeGBuffer();
    
    // Initialize MSAA G-Buffer
    bool InitializeMSAAGBuffer();
    
    // Initialize HDR framebuffer
    bool InitializeHDRBuffer();
    
    // Initialize Bloom buffers
    bool InitializeBloomBuffers();
    
    // Initialize fullscreen quad
    void InitializeQuad();
    
    // Initialize shaders
    bool InitializeShaders();

    // Initialize SSAO resources
    bool InitializeSSAO();
    
    // Compute SSAO using compute shader
    void ComputeSSAO();
};