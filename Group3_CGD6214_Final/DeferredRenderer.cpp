#include "DeferredRenderer.h"
#include <iostream>

DeferredRenderer::DeferredRenderer() : screenWidth(0), screenHeight(0), gBuffer(0), gPosition(0), gNormal(0), gAlbedoSpec(0), gDepth(0), gBufferMS(0), gPositionMS(0), gNormalMS(0), gAlbedoSpecMS(0), gDepthMS(0), msaaSamples(0), quadVAO(0), quadVBO(0), hdrFBO(0), hdrColorBuffer(0), hdrRBO(0), exposure(1.0f), bloomEnabled(true), bloomIntensity(0.8f) {
    pingpongFBO[0]=pingpongFBO[1]=0; pingpongColor[0]=pingpongColor[1]=0;
}
DeferredRenderer::~DeferredRenderer(){ Cleanup(); }

void DeferredRenderer::Cleanup(){
    auto delTex=[&](GLuint &t){ if(t){ glDeleteTextures(1,&t); t=0; }};
    auto delRB=[&](GLuint &r){ if(r){ glDeleteRenderbuffers(1,&r); r=0; }};
    auto delFB=[&](GLuint &f){ if(f){ glDeleteFramebuffers(1,&f); f=0; }};

    delTex(gPosition); delTex(gNormal); delTex(gAlbedoSpec); delRB(gDepth); delFB(gBuffer);
    delTex(gPositionMS); delTex(gNormalMS); delTex(gAlbedoSpecMS); delRB(gDepthMS); delFB(gBufferMS);
    delTex(hdrColorBuffer); delRB(hdrRBO); delFB(hdrFBO);
    for(int i=0;i<2;++i){ delTex(pingpongColor[i]); delFB(pingpongFBO[i]); }
    if(quadVBO){ glDeleteBuffers(1,&quadVBO); quadVBO=0; }
    if(quadVAO){ glDeleteVertexArrays(1,&quadVAO); quadVAO=0; }
}

bool DeferredRenderer::Initialize(int w,int h){ screenWidth=w; screenHeight=h; if(!InitializeGBuffer()) return false; if(msaaSamples>0 && !InitializeMSAAGBuffer()) return false; if(!InitializeHDRBuffer()) return false; if(!InitializeBloomBuffers()) return false; InitializeQuad(); if(!InitializeShaders()) return false; return true; }

void DeferredRenderer::SetMSAASamples(int samples){ samples = samples<0?0:samples; if(samples==msaaSamples) return; msaaSamples=samples; InitializeMSAAGBuffer(); }

bool DeferredRenderer::InitializeMSAAGBuffer(){
    // Create multi-sample gbuffer used only as render target; later resolved into single-sample textures
    if(msaaSamples<=0){ // free previous
        if(gBufferMS){ glDeleteFramebuffers(1,&gBufferMS); gBufferMS=0; }
        if(gPositionMS){ glDeleteTextures(1,&gPositionMS); gPositionMS=0; }
        if(gNormalMS){ glDeleteTextures(1,&gNormalMS); gNormalMS=0; }
        if(gAlbedoSpecMS){ glDeleteTextures(1,&gAlbedoSpecMS); gAlbedoSpecMS=0; }
        if(gDepthMS){ glDeleteRenderbuffers(1,&gDepthMS); gDepthMS=0; }
        return true;
    }
    if(gBufferMS){ glDeleteFramebuffers(1,&gBufferMS); gBufferMS=0; }
    glGenFramebuffers(1,&gBufferMS);
    glBindFramebuffer(GL_FRAMEBUFFER,gBufferMS);

    glGenTextures(1,&gPositionMS); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,gPositionMS); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGB16F, screenWidth, screenHeight, GL_TRUE); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D_MULTISAMPLE,gPositionMS,0);
    glGenTextures(1,&gNormalMS); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,gNormalMS); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGB16F, screenWidth, screenHeight, GL_TRUE); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D_MULTISAMPLE,gNormalMS,0);
    glGenTextures(1,&gAlbedoSpecMS); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,gAlbedoSpecMS); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGBA8, screenWidth, screenHeight, GL_TRUE); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D_MULTISAMPLE,gAlbedoSpecMS,0);

    glGenRenderbuffers(1,&gDepthMS); glBindRenderbuffer(GL_RENDERBUFFER,gDepthMS); glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_DEPTH_COMPONENT24, screenWidth, screenHeight); glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gDepthMS);

    GLuint attachments[3]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2}; glDrawBuffers(3,attachments);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){ std::cerr<<"MSAA GBuffer incomplete"<<std::endl; glBindFramebuffer(GL_FRAMEBUFFER,0); return false; }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return true;
}

bool DeferredRenderer::InitializeBloomBuffers(){ for(int i=0;i<2;++i){ if(pingpongFBO[i]) glDeleteFramebuffers(1,&pingpongFBO[i]); if(pingpongColor[i]) glDeleteTextures(1,&pingpongColor[i]); }
    glGenFramebuffers(2,pingpongFBO); glGenTextures(2,pingpongColor);
    for(int i=0;i<2;++i){ glBindFramebuffer(GL_FRAMEBUFFER,pingpongFBO[i]); glBindTexture(GL_TEXTURE_2D,pingpongColor[i]); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,screenWidth,screenHeight,0,GL_RGBA,GL_FLOAT,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,pingpongColor[i],0); if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){ std::cerr<<"Pingpong FBO incomplete"<<std::endl; return false; }} glBindFramebuffer(GL_FRAMEBUFFER,0); return true; }

bool DeferredRenderer::InitializeHDRBuffer(){ if(hdrFBO){ glDeleteFramebuffers(1,&hdrFBO); glDeleteTextures(1,&hdrColorBuffer); glDeleteRenderbuffers(1,&hdrRBO);} glGenFramebuffers(1,&hdrFBO); glBindFramebuffer(GL_FRAMEBUFFER,hdrFBO); glGenTextures(1,&hdrColorBuffer); glBindTexture(GL_TEXTURE_2D,hdrColorBuffer); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,screenWidth,screenHeight,0,GL_RGBA,GL_FLOAT,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,hdrColorBuffer,0); glGenRenderbuffers(1,&hdrRBO); glBindRenderbuffer(GL_RENDERBUFFER,hdrRBO); glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,screenWidth,screenHeight); glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,hdrRBO); if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){ std::cerr<<"HDR Framebuffer not complete!"<<std::endl; return false;} glBindFramebuffer(GL_FRAMEBUFFER,0); return true; }

bool DeferredRenderer::InitializeGBuffer(){ if(gBuffer){ glDeleteFramebuffers(1,&gBuffer); gBuffer=0; } if(gPosition){ glDeleteTextures(1,&gPosition); gPosition=0; } if(gNormal){ glDeleteTextures(1,&gNormal); gNormal=0; } if(gAlbedoSpec){ glDeleteTextures(1,&gAlbedoSpec); gAlbedoSpec=0; } if(gDepth){ glDeleteRenderbuffers(1,&gDepth); gDepth=0; }
    glGenFramebuffers(1,&gBuffer); glBindFramebuffer(GL_FRAMEBUFFER,gBuffer); glGenTextures(1,&gPosition); glBindTexture(GL_TEXTURE_2D,gPosition); glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,screenWidth,screenHeight,0,GL_RGB,GL_FLOAT,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gPosition,0); glGenTextures(1,&gNormal); glBindTexture(GL_TEXTURE_2D,gNormal); glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,screenWidth,screenHeight,0,GL_RGB,GL_FLOAT,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,gNormal,0); glGenTextures(1,&gAlbedoSpec); glBindTexture(GL_TEXTURE_2D,gAlbedoSpec); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,screenWidth,screenHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,gAlbedoSpec,0); glGenRenderbuffers(1,&gDepth); glBindRenderbuffer(GL_RENDERBUFFER,gDepth); glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,screenWidth,screenHeight); glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gDepth); GLuint attachments[3]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2}; glDrawBuffers(3,attachments); bool ok=(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE); if(!ok) std::cerr<<"DeferredRenderer: GBuffer incomplete"<<std::endl; glBindFramebuffer(GL_FRAMEBUFFER,0); return ok; }

void DeferredRenderer::InitializeQuad(){ if(quadVAO) return; float quadVerts[]={ -1.f,-1.f, 0.f,0.f, 1.f,-1.f, 1.f,0.f, 1.f, 1.f, 1.f,1.f, -1.f,-1.f, 0.f,0.f, 1.f, 1.f, 1.f,1.f, -1.f, 1.f, 0.f,1.f}; glGenVertexArrays(1,&quadVAO); glGenBuffers(1,&quadVBO); glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER,quadVBO); glBufferData(GL_ARRAY_BUFFER,sizeof(quadVerts),quadVerts,GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0); glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float))); glBindVertexArray(0); }

bool DeferredRenderer::InitializeShaders(){ geometryShader=std::make_unique<Shader>(); lightingShader=std::make_unique<Shader>(); debugShader=std::make_unique<Shader>(); hdrShader=std::make_unique<Shader>(); bloomExtractShader=std::make_unique<Shader>(); bloomBlurShader=std::make_unique<Shader>(); bool ok= geometryShader->LoadFromFile("shaders/gbuffer.vert","shaders/gbuffer.frag") && lightingShader->LoadFromFile("shaders/fullscreen_quad.vert","shaders/deferred_lighting.frag") && hdrShader->LoadFromFile("shaders/fullscreen_quad.vert","shaders/hdr.frag") && bloomExtractShader->LoadFromFile("shaders/fullscreen_quad.vert","shaders/bloom_extract.frag") && bloomBlurShader->LoadFromFile("shaders/fullscreen_quad.vert","shaders/bloom_blur.frag"); if(!ok) std::cerr<<"DeferredRenderer: Failed to load shaders."<<std::endl; return ok; }

void DeferredRenderer::BeginGeometryPass(){
    GLuint target = (msaaSamples>0)? gBufferMS : gBuffer;
    glBindFramebuffer(GL_FRAMEBUFFER,target); glViewport(0,0,screenWidth,screenHeight); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); geometryShader->Use(); }
void DeferredRenderer::EndGeometryPass(){ glBindFramebuffer(GL_FRAMEBUFFER,0); }

void DeferredRenderer::BeginLightingPass(){
    // If MSAA active, resolve multisample attachments into single-sample textures
    if(msaaSamples>0 && gBufferMS){
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferMS);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gBuffer);
        // Position
        glReadBuffer(GL_COLOR_ATTACHMENT0); glDrawBuffer(GL_COLOR_ATTACHMENT0); glBlitFramebuffer(0,0,screenWidth,screenHeight,0,0,screenWidth,screenHeight,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        // Normal
        glReadBuffer(GL_COLOR_ATTACHMENT1); glDrawBuffer(GL_COLOR_ATTACHMENT1); glBlitFramebuffer(0,0,screenWidth,screenHeight,0,0,screenWidth,screenHeight,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        // Albedo+Spec
        glReadBuffer(GL_COLOR_ATTACHMENT2); glDrawBuffer(GL_COLOR_ATTACHMENT2); glBlitFramebuffer(0,0,screenWidth,screenHeight,0,0,screenWidth,screenHeight,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        // Depth
        glBlitFramebuffer(0,0,screenWidth,screenHeight,0,0,screenWidth,screenHeight,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER,hdrFBO); glViewport(0,0,screenWidth,screenHeight); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); lightingShader->Use(); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,gPosition); lightingShader->SetInt("gPosition",0); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,gNormal); lightingShader->SetInt("gNormal",1); glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,gAlbedoSpec); lightingShader->SetInt("gAlbedoSpec",2); }

void DeferredRenderer::EndLightingPass(){
    // 1. Extract bright areas to pingpongColor[0]
    if(bloomEnabled){
        bloomExtractShader->Use();
        glBindFramebuffer(GL_FRAMEBUFFER,pingpongFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,hdrColorBuffer); bloomExtractShader->SetInt("scene",0); bloomExtractShader->SetFloat("threshold",1.0f); RenderQuad();

        // 2. Gaussian blur (ping-pong)
        bool horizontal=true; int blurPasses=8; bloomBlurShader->Use();
        for(int i=0;i<blurPasses;++i){
            glBindFramebuffer(GL_FRAMEBUFFER,pingpongFBO[horizontal]);
            bloomBlurShader->SetInt("horizontal", horizontal?1:0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (i==0)? pingpongColor[0] : pingpongColor[!horizontal]);
            bloomBlurShader->SetInt("image",0);
            RenderQuad();
            horizontal=!horizontal;
        }
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }

    // 3. Final composite + tone map to screen
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    hdrShader->Use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,hdrColorBuffer); hdrShader->SetInt("hdrBuffer",0);
    hdrShader->SetFloat("exposure",exposure);
    hdrShader->SetInt("useBloom", bloomEnabled?1:0);
    hdrShader->SetFloat("bloomIntensity", bloomIntensity);
    if(bloomEnabled){ glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, pingpongColor[!true]); hdrShader->SetInt("bloomBlur",1); }
    RenderQuad();
}

void DeferredRenderer::RenderQuad(){ glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES,0,6); glBindVertexArray(0); }

void DeferredRenderer::Resize(int newWidth,int newHeight){ if(newWidth==screenWidth && newHeight==screenHeight) return; screenWidth=newWidth; screenHeight=newHeight; InitializeGBuffer(); if(msaaSamples>0) InitializeMSAAGBuffer(); InitializeHDRBuffer(); InitializeBloomBuffers(); }

void DeferredRenderer::DebugDrawGBuffer(int){ }
