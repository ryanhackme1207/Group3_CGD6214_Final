#include "SimpleGUI.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

// 8x8 monochrome bitmap font (ensure this file exists in project)
#include "font8x8_basic.inl" // unsigned char font8x8_basic[128][8]

// Externals from main.cpp we will control
extern float timeOfDay;            // 0..24 hour cycle
extern bool useDirectionalLight;   // lighting toggle
extern int   MSAA;                 // msaa samples (0,2,4,8)
extern bool  gUseDeferred;         // deferred rendering toggle
extern class DeferredRenderer gDeferred; // renderer instance

// --------------------------------------------------------------------------------------
// Singleton
// --------------------------------------------------------------------------------------
SimpleGUI& SimpleGUI::Instance(){ static SimpleGUI g; return g; }

// --------------------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src){
    GLuint s = glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(s,512,nullptr,log); fprintf(stderr,"SimpleGUI shader compile error: %s\n",log);}    
    return s;
}

void SimpleGUI::buildShaders(){
    const char* vs = "#version 330 core\n"
                     "layout(location=0) in vec2 aPos;\n"
                     "layout(location=1) in vec2 aUV;\n"
                     "layout(location=2) in vec4 aCol;\n"
                     "out vec2 vUV; out vec4 vCol;\n"
                     "uniform mat4 uProj;\n"
                     "void main(){ vUV=aUV; vCol=aCol; gl_Position = uProj * vec4(aPos,0,1); }";
    const char* fs = "#version 330 core\n"
                     "in vec2 vUV; in vec4 vCol; out vec4 FragColor; uniform sampler2D uTex;\n"
                     "void main(){ vec4 tex = texture(uTex,vUV); FragColor = vCol*tex; }";
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    m_shader = glCreateProgram();
    glAttachShader(m_shader, v);
    glAttachShader(m_shader, f);
    glLinkProgram(m_shader);
    glDeleteShader(v); glDeleteShader(f);
    m_uProj = glGetUniformLocation(m_shader, "uProj");
    m_uTex  = glGetUniformLocation(m_shader, "uTex");
    glGenVertexArrays(1,&m_VAO);
    glGenBuffers(1,&m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, 4 * 6 * 1000 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW); // reserve
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(2*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vertex),(void*)(4*sizeof(float)));
    glBindVertexArray(0);
}

void SimpleGUI::buildFont(){
    // Pack ASCII 0..127 horizontally (width = 128*8, height = 8)
    const int glyphW=8, glyphH=8; const int cols=128; int texW = cols*glyphW; int texH=glyphH;
    std::vector<unsigned char> pixels(texW*texH, 0);
    for(int c=0;c<128;++c){
        for(int row=0; row<8; ++row){ unsigned char bits = font8x8_basic[c][row];
            for(int col=0; col<8; ++col){ if(bits & (1<<col)){ int x=c*8+col; int y=row; pixels[y*texW + x] = 255; } }
        }
    }
    glGenTextures(1,&m_fontTex);
    glBindTexture(GL_TEXTURE_2D,m_fontTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RED,texW,texH,0,GL_RED,GL_UNSIGNED_BYTE,pixels.data());
}

void SimpleGUI::Initialize(GLFWwindow* window){
    if(m_initialized) return; m_window = window; buildShaders(); buildFont(); m_initialized = true; }

void SimpleGUI::BeginFrame(){
    if(!m_initialized) return; 
    glfwGetCursorPos(m_window,&m_mouseX,&m_mouseY);
    int state = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
    m_mousePressed = (state==GLFW_PRESS && !m_mouseDown);
    m_mouseDown = (state==GLFW_PRESS);
    m_vertices.clear();
    // Reset layout
    m_cursorX = 15.f; m_cursorY = 25.f; // leave margin for dragging etc.
}

bool SimpleGUI::regionHit(float x,float y,float w,float h) const { return m_mouseX>=x && m_mouseX<=x+w && m_mouseY>=y && m_mouseY<=y+h; }

// Add one quad as two triangles (6 vertices)
void SimpleGUI::addQuad(float x,float y,float w,float h,uint32_t rgba,float u0,float v0,float u1,float v1){
    Vertex v; v.color = rgba;
    // tri 1
    v.x=x; v.y=y; v.u=u0; v.v=v0; m_vertices.push_back(v);
    v.x=x+w; v.y=y; v.u=u1; v.v=v0; m_vertices.push_back(v);
    v.x=x+w; v.y=y+h; v.u=u1; v.v=v1; m_vertices.push_back(v);
    // tri 2
    v.x=x; v.y=y; v.u=u0; v.v=v0; m_vertices.push_back(v);
    v.x=x+w; v.y=y+h; v.u=u1; v.v=v1; m_vertices.push_back(v);
    v.x=x; v.y=y+h; v.u=u0; v.v=v1; m_vertices.push_back(v);
}

void SimpleGUI::addRect(float x,float y,float w,float h,uint32_t rgba){ addQuad(x,y,w,h,rgba,0,0,0,0); }

void SimpleGUI::addText(float x,float y,const std::string& text,uint32_t rgba){
    const int glyphW=8,glyphH=8; float tu = 1.0f/(128.0f*glyphW); // Actually width is 128*8 pixels, treat each glyph width = 8 pixels => normalized width = 8/(128*8)=1/128
    float glyphUWidth = 1.0f/128.0f; // easier: each char occupies 1/128 of width
    for(size_t i=0;i<text.size();++i){ unsigned char c = (unsigned char)text[i];
        if(c>=128) continue; float u0 = c*glyphUWidth; float u1 = u0 + glyphUWidth; addQuad(x + i*glyphW, y, (float)glyphW, (float)glyphH, rgba, u0, 0.0f, u1, 1.0f); }
}

void SimpleGUI::Label(const std::string& text){ addText(m_cursorX, m_cursorY, text, 0xFFFFFFFF); m_cursorY += m_lineHeight; }

bool SimpleGUI::Button(const std::string& label, float w){
    float width = (w>0? w : m_panelWidth - 30.f); float h=18.f; float x=m_cursorX; float y=m_cursorY; bool hov=regionHit(x,y,width,h);
    uint32_t col = hov?0xFF5A5A90:0xFF3A3A60; addRect(x,y,width,h,col); addText(x+6,y+5,label,0xFFFFFFFF);
    bool clicked = hov && m_mousePressed; m_cursorY += h + 4.f; return clicked; }

bool SimpleGUI::Toggle(const std::string& label, bool& value){
    float h=16.f; float x=m_cursorX; float y=m_cursorY; float box=14.f; bool hov = regionHit(x,y,box,h); uint32_t boxCol = value?0xFF2BAA55:0xFF666666; if(hov) boxCol ^= 0x00222222; addRect(x,y,box,box,boxCol); if(value) addText(x+3,y+3,"X",0xFFFFFFFF); addText(x+box+6,y+2,label,0xFFFFFFFF); if(hov && m_mousePressed) value=!value; m_cursorY += h + 6.f; return value; }

bool SimpleGUI::SliderFloat(const std::string& label, float& value, float minV, float maxV, float step){
    // Label line
    addText(m_cursorX, m_cursorY, label, 0xFFE0E0E0); m_cursorY += 12.f;
    float x = m_cursorX; float y = m_cursorY; float w = m_panelWidth - 30.f; float h = 14.f; addRect(x,y,w,h,0xFF202020);
    float norm = (value - minV)/(maxV - minV); norm = std::max(0.f,std::min(1.f,norm));
    float knobW = 10.f; float knobX = x + norm*(w - knobW); bool hov = regionHit(x,y,w,h); bool drag = hov && m_mouseDown; uint32_t kcol = hov?0xFF8888CC:0xFF666699; addRect(knobX,y,knobW,h,kcol);
    if(drag){ double mx = m_mouseX; if(mx < x) mx=x; if(mx > x+w) mx=x+w; norm = float((mx - x)/(w)); value = minV + norm*(maxV - minV); if(step>0){ float steps = roundf((value-minV)/step); value = minV + steps*step; value = std::max(minV,std::min(maxV,value)); }}
    char buf[64]; snprintf(buf,64,"%.2f", value); addText(x+w- (float)strlen(buf)*8.f - 2.f, y-10.f, buf, 0xFFB0FFB0);
    m_cursorY += h + 8.f; return drag; }

bool SimpleGUI::SliderInt(const std::string& label, int& value, int minV, int maxV){ float f=(float)value; bool ch = SliderFloat(label,f,(float)minV,(float)maxV,1.0f); int nv=(int)roundf(f); if(nv!=value){ value=nv; ch=true;} return ch; }

void SimpleGUI::buildPanels(float deltaTime){
    // Panel background (auto height; we estimate after building so just draw big rect behind first)
    float panelX = 10.f; float panelY=10.f; float panelW = m_panelWidth; // height dynamic
    // We'll accumulate vertices to temporary list then later we could patch background; easiest: draw background first of generous height
    addRect(panelX, panelY, panelW, 450.f, 0xAA101018);
    m_cursorX = panelX + 10.f; m_cursorY = panelY + 10.f;

    // FPS statistics
    m_timeAccum += deltaTime; ++m_frameCount; if(m_timeAccum >= 0.5){ m_fps = (float)(m_frameCount / m_timeAccum); m_frameCount=0; m_timeAccum = 0.0; }
    char stats[64]; snprintf(stats,64,"FPS: %.1f", m_fps); Label(stats);

    // Deferred toggle
    bool wasDeferred = gUseDeferred; Toggle("Deferred Rendering", gUseDeferred);
    if(gUseDeferred && !wasDeferred){ // initialize if needed
        // Query current viewport size
        GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp); gDeferred.Initialize(vp[2], vp[3]); }
    if(gUseDeferred){
        float exp = gDeferred.GetExposure(); SliderFloat("HDR Exposure", exp, 0.1f, 5.0f, 0.05f); gDeferred.SetExposure(exp);
        bool bloom = gDeferred.IsBloomEnabled(); Toggle("Bloom", bloom); gDeferred.SetBloomEnabled(bloom);
        float bi = gDeferred.GetBloomIntensity(); SliderFloat("Bloom Intensity", bi, 0.0f, 5.0f, 0.05f); gDeferred.SetBloomIntensity(bi);
    }

    // Time of day slider (wrap maintained by main loop)
    SliderFloat("Time Of Day", timeOfDay, 0.0f, 24.0f, 0.01f);

    // Directional light toggle
    Toggle("Directional Light", useDirectionalLight);

    // MSAA selection (0/2/4/8) using an int slider but we clamp to allowed set
    int msaaVal = MSAA; if(msaaVal!=0 && msaaVal!=2 && msaaVal!=4 && msaaVal!=8) msaaVal = 0;
    SliderInt("MSAA Samples", msaaVal, 0, 8); // user can drag through numbers; we snap after
    // Snap to allowed values (0,2,4,8)
    int allowed[4] = {0,2,4,8}; int closest = 0; int bestDiff = 9999; for(int a: allowed){ int d=std::abs(a-msaaVal); if(d<bestDiff){ bestDiff=d; closest=a; }}
    if(closest != MSAA){ MSAA = closest; if(MSAA==0) glDisable(GL_MULTISAMPLE); else glEnable(GL_MULTISAMPLE); if(gUseDeferred) gDeferred.SetMSAASamples(MSAA); }

    Label("F1: Toggle GUI");
}

void SimpleGUI::flush(){
    if(m_vertices.empty()) return;
    // Determine viewport for projection
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp); float L=(float)vp[0]; float T=(float)vp[1]; float W=(float)vp[2]; float H=(float)vp[3];
    float R=L+W; float B=T+H;
    float proj[16] = { 2/(R-L),0,0,0, 0,2/(T-B),0,0, 0,0,-1,0, (R+L)/(L-R),(T+B)/(B-T),0,1 };
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
    glUseProgram(m_shader); glUniform1i(m_uTex,0); glUniformMatrix4fv(m_uProj,1,GL_FALSE,proj);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,m_fontTex);
    glBindVertexArray(m_VAO); glBindBuffer(GL_ARRAY_BUFFER,m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER,0,m_vertices.size()*sizeof(Vertex), m_vertices.data());
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)m_vertices.size());
    glBindVertexArray(0); glDisable(GL_BLEND);
}

void SimpleGUI::Draw(float deltaTime){ if(!m_initialized || !m_visible) return; buildPanels(deltaTime); flush(); }
