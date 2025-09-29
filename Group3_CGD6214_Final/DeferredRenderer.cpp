#include "DeferredRenderer.h"
#include <iostream>

DeferredRenderer::DeferredRenderer() : screenWidth(0), screenHeight(0), gBuffer(0), gPosition(0), gNormal(0), gAlbedoSpec(0), gDepth(0), quadVAO(0), quadVBO(0) {}
DeferredRenderer::~DeferredRenderer(){ Cleanup(); }

bool DeferredRenderer::Initialize(int w, int h){ screenWidth=w; screenHeight=h; if(!InitializeGBuffer()) return false; InitializeQuad(); if(!InitializeShaders()) return false; return true; }

void DeferredRenderer::Cleanup(){ if(gPosition) glDeleteTextures(1,&gPosition); if(gNormal) glDeleteTextures(1,&gNormal); if(gAlbedoSpec) glDeleteTextures(1,&gAlbedoSpec); if(gDepth) glDeleteRenderbuffers(1,&gDepth); if(gBuffer) glDeleteFramebuffers(1,&gBuffer); gBuffer=0; gPosition=gNormal=gAlbedoSpec=gDepth=0; if(quadVBO) glDeleteBuffers(1,&quadVBO); if(quadVAO) glDeleteVertexArrays(1,&quadVAO); quadVAO=quadVBO=0; }

bool DeferredRenderer::InitializeGBuffer(){
    if(gBuffer){ Cleanup(); }
    glGenFramebuffers(1,&gBuffer); glBindFramebuffer(GL_FRAMEBUFFER,gBuffer);
    // Position
    glGenTextures(1,&gPosition); glBindTexture(GL_TEXTURE_2D,gPosition); glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,screenWidth,screenHeight,0,GL_RGB,GL_FLOAT,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gPosition,0);
    // Normal
    glGenTextures(1,&gNormal); glBindTexture(GL_TEXTURE_2D,gNormal); glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,screenWidth,screenHeight,0,GL_RGB,GL_FLOAT,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,gNormal,0);
    // Albedo + Spec
    glGenTextures(1,&gAlbedoSpec); glBindTexture(GL_TEXTURE_2D,gAlbedoSpec); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,screenWidth,screenHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,gAlbedoSpec,0);
    // Depth (renderbuffer)
    glGenRenderbuffers(1,&gDepth); glBindRenderbuffer(GL_RENDERBUFFER,gDepth); glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,screenWidth,screenHeight); glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gDepth);
    GLuint attachments[3]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2}; glDrawBuffers(3,attachments);
    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE); if(!ok) std::cerr<<"DeferredRenderer: GBuffer incomplete"<<std::endl; glBindFramebuffer(GL_FRAMEBUFFER,0); return ok; }

void DeferredRenderer::InitializeQuad(){ if(quadVAO) return; float quadVerts[]={ // positions   // texcoords
        -1.f,-1.f, 0.f,0.f,
         1.f,-1.f, 1.f,0.f,
         1.f, 1.f, 1.f,1.f,
        -1.f,-1.f, 0.f,0.f,
         1.f, 1.f, 1.f,1.f,
        -1.f, 1.f, 0.f,1.f};
    glGenVertexArrays(1,&quadVAO); glGenBuffers(1,&quadVBO); glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER,quadVBO); glBufferData(GL_ARRAY_BUFFER,sizeof(quadVerts),quadVerts,GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0); glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float))); glBindVertexArray(0); }

bool DeferredRenderer::InitializeShaders(){ geometryShader = std::make_unique<Shader>(); lightingShader = std::make_unique<Shader>(); debugShader = std::make_unique<Shader>(); bool ok = geometryShader->LoadFromFile("shaders/gbuffer.vert","shaders/gbuffer.frag") && lightingShader->LoadFromFile("shaders/fullscreen_quad.vert","shaders/deferred_lighting.frag"); if(!ok) std::cerr<<"DeferredRenderer: Failed to load shaders."<<std::endl; return ok; }

void DeferredRenderer::BeginGeometryPass(){ glBindFramebuffer(GL_FRAMEBUFFER,gBuffer); glViewport(0,0,screenWidth,screenHeight); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); geometryShader->Use(); }
void DeferredRenderer::EndGeometryPass(){ glBindFramebuffer(GL_FRAMEBUFFER,0); }

void DeferredRenderer::BeginLightingPass(){ glViewport(0,0,screenWidth,screenHeight); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); lightingShader->Use(); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,gPosition); lightingShader->SetInt("gPosition",0); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,gNormal); lightingShader->SetInt("gNormal",1); glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,gAlbedoSpec); lightingShader->SetInt("gAlbedoSpec",2); }
void DeferredRenderer::EndLightingPass(){ }

void DeferredRenderer::RenderQuad(){ glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES,0,6); glBindVertexArray(0); }

void DeferredRenderer::Resize(int newWidth, int newHeight){ if(newWidth==screenWidth && newHeight==screenHeight) return; screenWidth=newWidth; screenHeight=newHeight; InitializeGBuffer(); }

void DeferredRenderer::DebugDrawGBuffer(int){ /* optional */ }
