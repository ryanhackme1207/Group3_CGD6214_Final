#include "SimpleGUI.h"
#include "DeferredRenderer.h"
#include "SceneNode.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>
#include <string>

// Embedded 8x8 bitmap font (ASCII 0..127). Only printable 32..126 populated.
static const unsigned char SIMPLEGUI_FONT[128][8] = {
// 0-31 control = blank
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
// 32 space
{0,0,0,0,0,0,0,0}, {24,24,24,24,24,0,24,0}, {54,54,20,0,0,0,0,0}, {54,54,127,54,127,54,54,0},
{24,62,3,30,48,31,24,0}, {70,102,12,24,48,102,98,0}, {28,54,28,55,110,102,59,0}, {6,6,12,0,0,0,0,0},
{24,12,6,6,6,12,24,0}, {6,12,24,24,24,12,6,0}, {0,102,60,255,60,102,0,0}, {0,12,12,63,12,12,0,0},
{0,0,0,0,0,12,12,24}, {0,0,0,63,0,0,0,0}, {0,0,0,0,0,12,12,0}, {96,96,48,24,12,6,3,0},
{62,99,115,123,111,103,62,0}, {12,14,12,12,12,12,63,0}, {30,51,48,28,6,51,63,0}, {30,51,48,28,48,51,30,0},
{56,60,54,51,127,48,120,0}, {63,3,31,48,48,51,30,0}, {28,6,3,31,51,51,30,0}, {63,49,48,24,12,12,12,0},
{30,51,51,30,51,51,30,0}, {30,51,51,62,48,24,14,0}, {0,12,12,0,0,12,12,0}, {0,12,12,0,0,12,12,24},
{48,24,12,6,12,24,48,0}, {0,0,63,0,0,63,0,0}, {6,12,24,48,24,12,6,0}, {30,51,48,24,12,0,12,0},
{62,99,123,123,123,3,30,0}, {12,30,51,51,63,51,51,0}, {31,51,51,31,51,51,31,0}, {28,54,3,3,3,54,28,0},
{15,27,51,51,51,27,15,0}, {63,51,11,15,11,51,63,0}, {63,51,11,15,11,3,3,0}, {28,54,3,59,51,54,60,0},
{51,51,51,63,51,51,51,0}, {30,12,12,12,12,12,30,0}, {60,24,24,24,27,27,14,0}, {51,27,15,7,15,27,51,0},
{3,3,3,3,3,51,63,0}, {33,51,63,63,51,51,51,0}, {51,55,55,63,59,59,51,0}, {30,51,51,51,51,51,30,0},
{31,51,51,31,3,3,3,0}, {30,51,51,51,51,59,30,56}, {31,51,51,31,15,27,51,0}, {30,51,3,30,48,51,30,0},
{63,63,45,12,12,12,30,0}, {51,51,51,51,51,51,30,0}, {51,51,51,51,51,30,12,0}, {51,51,51,63,63,63,18,0},
{51,51,30,12,30,51,51,0}, {51,51,30,12,12,12,30,0}, {63,49,24,12,6,51,63,0}, {30,6,6,6,6,6,30,0},
{3,6,12,24,48,32,0,0}, {30,24,24,24,24,24,30,0}, {8,28,54,0,0,0,0,0}, {0,0,0,0,0,0,0,255},
{6,6,12,0,0,0,0,0}, {0,0,30,48,62,51,110,0}, {3,3,3,31,51,51,31,0}, {0,0,30,51,3,51,30,0}, {48,48,48,62,51,51,62,0},
{0,0,30,51,63,3,30,0}, {28,54,6,15,6,6,15,0}, {0,0,60,51,51,62,48,30}, {3,3,27,55,51,51,51,0}, {12,0,14,12,12,12,30,0},
{24,0,28,24,24,24,24,15}, {3,3,51,27,15,27,51,0}, {14,12,12,12,12,12,30,0}, {0,0,27,63,63,51,51,0}, {0,0,31,51,51,51,51,0},
{0,0,30,51,51,51,30,0}, {0,0,31,51,51,31,3,3}, {0,0,62,51,51,62,48,48}, {0,0,29,55,51,3,3,0}, {0,0,62,3,30,48,31,0},
{4,6,63,6,6,54,28,0}, {0,0,51,51,51,51,62,0}, {0,0,51,51,51,30,12,0}, {0,0,51,63,63,63,18,0}, {0,0,51,30,12,30,51,0},
{0,0,51,51,51,62,48,30}, {0,0,63,24,12,6,63,0}, {56,12,12,7,12,12,56,0}, {12,12,12,0,12,12,12,0}, {7,12,12,56,12,12,7,0},
{38,45,24,0,0,0,0,0}, {0,0,0,0,0,0,0,0}
};

// Forward declarations of existing externs
extern float timeOfDay;            // 0..24 hour cycle
extern bool useDirectionalLight;   // lighting toggle
extern int   MSAA;                 // msaa samples (0,2,4,8)
extern bool  gUseDeferred;         // deferred rendering toggle
extern DeferredRenderer gDeferred; // renderer instance

SimpleGUI& SimpleGUI::Instance(){ static SimpleGUI g; return g; }

static GLuint sgCompile(GLenum type, const char* src){ GLuint s=glCreateShader(type); glShaderSource(s,1,&src,nullptr); glCompileShader(s); GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok); if(!ok){ char log[512]; glGetShaderInfoLog(s,512,nullptr,log); fprintf(stderr,"SimpleGUI shader error: %s\n",log);} return s; }

void SimpleGUI::buildShaders(){
    const char* vs = "#version 330 core\nlayout(location=0) in vec2 aPos;layout(location=1) in vec2 aUV;layout(location=2) in vec4 aCol;out vec2 vUV;out vec4 vCol;uniform mat4 uProj;void main(){vUV=aUV;vCol=aCol;gl_Position=uProj*vec4(aPos,0,1);}";
    const char* fs = "#version 330 core\nin vec2 vUV;in vec4 vCol;out vec4 FragColor;uniform sampler2D uTex;void main(){FragColor=vCol*texture(uTex,vUV);}";
    GLuint V=sgCompile(GL_VERTEX_SHADER,vs), F=sgCompile(GL_FRAGMENT_SHADER,fs); m_shader=glCreateProgram(); glAttachShader(m_shader,V); glAttachShader(m_shader,F); glLinkProgram(m_shader); glDeleteShader(V); glDeleteShader(F); m_uProj=glGetUniformLocation(m_shader,"uProj"); m_uTex=glGetUniformLocation(m_shader,"uTex");
    glGenVertexArrays(1,&m_VAO); glGenBuffers(1,&m_VBO); glBindVertexArray(m_VAO); glBindBuffer(GL_ARRAY_BUFFER,m_VBO); glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*6*2000, nullptr, GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0); glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(2*sizeof(float))); glEnableVertexAttribArray(2); glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vertex),(void*)(4*sizeof(float))); glBindVertexArray(0);
}

void SimpleGUI::buildFont(){
    const int glyphW=8,glyphH=8; int texW=128*glyphW, texH=glyphH; std::vector<unsigned char> pixels(texW*texH,0);
    for(int c=0;c<128;++c){ for(int row=0;row<8;++row){ unsigned char bits=SIMPLEGUI_FONT[c][row]; for(int col=0; col<8; ++col){ if(bits & (1<<col)){ int x=c*8+col; int y=row; pixels[y*texW+x]=255; } } } }
    glGenTextures(1,&m_fontTex); glBindTexture(GL_TEXTURE_2D,m_fontTex); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); glTexImage2D(GL_TEXTURE_2D,0,GL_RED,texW,texH,0,GL_RED,GL_UNSIGNED_BYTE,pixels.data());
}

void SimpleGUI::Initialize(GLFWwindow* w){ if(m_initialized) return; m_window=w; buildShaders(); buildFont(); m_initialized=true; }

void SimpleGUI::BeginFrame(){
    if(!m_initialized) return;
    glfwGetCursorPos(m_window,&m_mouseX,&m_mouseY);
    int st=glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
    m_mousePressed=(st==GLFW_PRESS && !m_mouseDown);
    m_mouseDown=(st==GLFW_PRESS);
    m_vertices.clear();
    // per-frame key edge detection for Z/X
    int zState = glfwGetKey(m_window, GLFW_KEY_Z);
    int xState = glfwGetKey(m_window, GLFW_KEY_X);
    m_zPressedFrame = (zState==GLFW_PRESS && !m_prevZ);
    m_xPressedFrame = (xState==GLFW_PRESS && !m_prevX);
    m_prevZ = (zState==GLFW_PRESS);
    m_prevX = (xState==GLFW_PRESS);
}

void SimpleGUI::recordFrameTime(float dt){
    float ms = dt * 1000.0f;
    m_frameTimes[m_frameIndex++] = ms;
    if(m_frameIndex >= FRAME_HISTORY){ m_frameIndex = 0; m_historyFilled = true; }
    int count = m_historyFilled ? FRAME_HISTORY : m_frameIndex;
    if(count==0) return;
    m_minFrameMs = m_frameTimes[0]; m_maxFrameMs = m_frameTimes[0]; double sum=0.0;
    for(int i=0;i<count;++i){ float v=m_frameTimes[i]; if(v<m_minFrameMs) m_minFrameMs=v; if(v>m_maxFrameMs) m_maxFrameMs=v; sum+=v; }
    m_avgFrameMs = (float)(sum / count);
}

void SimpleGUI::buildPerformancePanel(float deltaTime){
    recordFrameTime(deltaTime);
    float startX = m_panelWidth + 30.f + m_panelWidth + 40.f; // third panel to the right
    float panelW = m_panelWidth + 40.f;
    float panelH = 180.f;
    addRect(startX,5,panelW+10,panelH+140,0xAA101018);
    addText(startX+15,10,"Performance",0xFFFFFFAA);
    char line[128];
    std::snprintf(line,128,"Avg: %.2f ms (%.0f FPS)", m_avgFrameMs, (m_avgFrameMs>0? 1000.0f/m_avgFrameMs:0.f));
    addText(startX+15,30,line,0xFFE0E0E0);
    std::snprintf(line,128,"Min: %.2f ms  Max: %.2f ms", m_minFrameMs, m_maxFrameMs);
    addText(startX+15,46,line,0xFFB0B0B0);
    // Graph area
    float graphX = startX + 15.f;
    float graphY = 66.f;
    float graphW = panelW - 30.f;
    float graphH = 90.f;
    addRect(graphX,graphY,graphW,graphH,0xFF1A1A30);
    int count = m_historyFilled ? FRAME_HISTORY : m_frameIndex;
    if(count>1){
        float maxV = m_maxFrameMs; if(maxV < 16.f) maxV = 16.f; // ensure visible
        float barW = graphW / (float)FRAME_HISTORY;
        for(int i=0;i<count;++i){ int idx = (m_historyFilled? (m_frameIndex + i) % FRAME_HISTORY : i); float v = m_frameTimes[idx]; float norm = v / maxV; if(norm>1.f) norm=1.f; float h = norm * (graphH-4.f); float bx = graphX + i * barW; float by = graphY + graphH - 2.f - h; uint32_t col = (v > 33.0f)?0xFFAA3333: (v>22.f?0xFFAA8833:0xFF33AA55); addRect(bx,by,barW-1.f,h,col);
        }
        // reference lines (16.6ms ~60fps, 33.3ms ~30fps)
        float ref60Y = graphY + graphH - 2.f - (16.6f / maxV) * (graphH-4.f);
        float ref30Y = graphY + graphH - 2.f - (33.3f / maxV) * (graphH-4.f);
        addRect(graphX,ref60Y,graphW,1.f,0x40FFFFFF);
        addRect(graphX,ref30Y,graphW,1.f,0x40FFAA66);
        addText(graphX+graphW-50.f,ref60Y-8.f,"60fps",0x80FFFFFF);
        addText(graphX+graphW-50.f,ref30Y-8.f,"30fps",0x80FFCC99);
    }
    // instructions
    addText(startX+15,graphY+graphH+8.f,"Bars show last ~2s frame times",0xFF9999CC);
}

// remove duplicate BeginFrame earlier if present and implement unified version with performance capture
// void SimpleGUI::BeginFrame(){ if(!m_initialized) return; glfwGetCursorPos(m_window,&m_mouseX,&m_mouseY); int st=glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT); m_mousePressed=(st==GLFW_PRESS && !m_mouseDown); m_mouseDown=(st==GLFW_PRESS); m_vertices.clear(); }

bool SimpleGUI::regionHit(float x,float y,float w,float h) const { return m_mouseX>=x && m_mouseX<=x+w && m_mouseY>=y && m_mouseY<=y+h; }

void SimpleGUI::addQuad(float x,float y,float w,float h,uint32_t rgba,float u0,float v0,float u1,float v1){ Vertex v; v.color=rgba; v.x=x; v.y=y; v.u=u0; v.v=v0; m_vertices.push_back(v); v.x=x+w; v.y=y; v.u=u1; v.v=v0; m_vertices.push_back(v); v.x=x+w; v.y=y+h; v.u=u1; v.v=v1; m_vertices.push_back(v); v.x=x; v.y=y; v.u=u0; v.v=v0; m_vertices.push_back(v); v.x=x+w; v.y=y+h; v.u=u1; v.v=v1; m_vertices.push_back(v); v.x=x; v.y=y+h; v.u=u0; v.v=v1; m_vertices.push_back(v); }
void SimpleGUI::addRect(float x,float y,float w,float h,uint32_t rgba){ addQuad(x,y,w,h,rgba,0,0,0,0); }

void SimpleGUI::addText(float x,float y,const std::string& text,uint32_t rgba){ float gw=8.f; float gu=1.f/128.f; for(size_t i=0;i<text.size();++i){ unsigned char c=(unsigned char)text[i]; if(c>=128) continue; float u0=c*gu; float u1=u0+gu; addQuad(x+i*gw,y,gw,8.f,rgba,u0,0,u1,1); } }

void SimpleGUI::Label(const std::string& t){ addText(m_cursorX,m_cursorY,t,0xFFFFFFFF); m_cursorY+=m_lineHeight; }

bool SimpleGUI::Button(const std::string& label,float w){ float bw = (w>0? w : m_panelWidth-20.f); float h=18.f; float x=m_cursorX; float y=m_cursorY; bool hov=regionHit(x,y,bw,h); addRect(x,y,bw,h, hov?0xFF4A4A90:0xFF2A2A60); addText(x+6,y+5,label); bool clk=hov&&m_mousePressed; m_cursorY+=h+4.f; return clk; }

bool SimpleGUI::Toggle(const std::string& label,bool& value){ float h=16.f; float x=m_cursorX; float y=m_cursorY; float box=14.f; bool hov=regionHit(x,y,box,h); uint32_t col=value?0xFF2BAA55:0xFF666666; if(hov) col^=0x00222222; addRect(x,y,box,box,col); if(value) addText(x+3,y+3,"X"); addText(x+box+6,y+2,label); if(hov&&m_mousePressed) value=!value; m_cursorY+=h+6.f; return value; }

bool SimpleGUI::SliderFloat(const std::string& label,float& value,float minV,float maxV,float step){ addText(m_cursorX,m_cursorY,label,0xFFE0E0E0); m_cursorY+=12.f; float x=m_cursorX; float y=m_cursorY; float w=m_panelWidth-20.f; float h=14.f; addRect(x,y,w,h,0xFF202020); float norm=(value-minV)/(maxV-minV); if(norm<0) norm=0; if(norm>1) norm=1; float knobW=10.f; float kx=x+norm*(w-knobW); bool hov=regionHit(x,y,w,h); if(hov && m_mouseDown){ double mx=m_mouseX; if(mx<x) mx=x; if(mx>x+w) mx=x+w; norm=float((mx-x)/w); value=minV+norm*(maxV-minV); if(step>0){ float stp=roundf((value-minV)/step); value=minV+stp*step; value=std::max(minV,std::min(maxV,value)); }} addRect(kx,y,knobW,h, hov?0xFF8888CC:0xFF666699); char buf[48]; std::snprintf(buf,48,"%.2f",value); addText(x+w- (float)strlen(buf)*8.f -2.f,y-10.f,buf,0xFFB0FFB0); m_cursorY+=h+8.f; return hov; }

bool SimpleGUI::SliderInt(const std::string& label,int& value,int minV,int maxV){ float f=(float)value; bool ch=SliderFloat(label,f,(float)minV,(float)maxV,1.f); int nv=(int)roundf(f); if(nv!=value){ value=nv; ch=true;} return ch; }

void SimpleGUI::openAll(SceneNode* n){ if(!n) return; m_openState[n]=true; for(auto &c: n->GetChildren()) openAll(c.get()); }
void SimpleGUI::closeAll(SceneNode* n){ if(!n) return; m_openState[n]=false; for(auto &c: n->GetChildren()) closeAll(c.get()); }

void SimpleGUI::drawNodeRecursive(SceneNode* node,int depth,float& y,float panelX,float panelW){
    if(!node) return; float lineH=16.f; float rowY=y; float rowHeight=lineH; float xLabel=panelX+10.f+depth*14.f; bool hasChildren=!node->GetChildren().empty(); bool open=m_openState[node]; bool hovered=regionHit(panelX+8.f,rowY,panelW-16.f,rowHeight);
    uint32_t col = (m_selectedNode==node?0xFF5560AA: hovered?0xFF3A3A80:0xFF2A2A60); addRect(panelX+8.f,rowY,panelW-16.f,rowHeight,col);
    if(hasChildren){ uint32_t ic = open?0xFF88FF88:0xFFFF8888; addRect(panelX+12.f+depth*14.f,rowY+5.f,8.f,8.f,ic); }
    addText(xLabel + (hasChildren?14.f:0.f), rowY+4.f, node->name.empty()?"(unnamed)":node->name.c_str());
    if(hovered && m_mousePressed) m_selectedNode=node;
    // Use per-frame key edges; apply when row hovered or selected
    if((hovered || m_selectedNode==node) && hasChildren){ if(m_zPressedFrame){ m_openState[node]=true; open=true; } if(m_xPressedFrame){ m_openState[node]=false; open=false; } }
    y += rowHeight + 2.f; if(open){ for(auto &c : node->GetChildren()) drawNodeRecursive(c.get(), depth+1,y,panelX,panelW); }
}

void SimpleGUI::buildHierarchyPanel(){
    if(!m_sceneRoot) return; if(!m_autoExpandedOnce){ openAll(m_sceneRoot); m_autoExpandedOnce=true; }
    float startX = m_panelWidth + 30.f; float panelW = m_panelWidth; float y = 15.f; float panelH = 360.f; addRect(startX,5,panelW+10,panelH+40,0xAA101018); addText(startX+15,10,"Scene Hierarchy (Z expand / X collapse)",0xFFFFFFAA); y += 24.f; drawNodeRecursive(m_sceneRoot,0,y,startX,panelW); if(m_selectedNode){ char buf[128]; std::snprintf(buf,128,"Selected: %s", m_selectedNode->name.c_str()); addText(startX+15,panelH+18,buf,0xFFCCCCFF); }
}

void SimpleGUI::buildPanels(float dt){
    addRect(5,5,m_panelWidth+10,500,0xAA101018); m_cursorX=15; m_cursorY=15; m_timeAccum+=dt; ++m_frameCount; if(m_timeAccum>=0.5){ m_fps=(float)(m_frameCount/m_timeAccum); m_timeAccum=0; m_frameCount=0; } char s[64]; std::snprintf(s,64,"FPS: %.1f",m_fps); Label(s); bool prev=gUseDeferred; Toggle("Deferred Rendering", gUseDeferred); if(gUseDeferred && !prev){ GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp); gDeferred.Initialize(vp[2],vp[3]); }
    if(gUseDeferred){ float exp=gDeferred.GetExposure(); SliderFloat("HDR Exposure",exp,0.1f,5.f,0.05f); gDeferred.SetExposure(exp); bool bloom=gDeferred.IsBloomEnabled(); Toggle("Bloom",bloom); gDeferred.SetBloomEnabled(bloom); float bi=gDeferred.GetBloomIntensity(); SliderFloat("Bloom Intensity",bi,0.f,5.f,0.05f); gDeferred.SetBloomIntensity(bi); }
    SliderFloat("Time Of Day", timeOfDay,0.f,24.f,0.01f); Toggle("Directional Light", useDirectionalLight); int msaaVal=MSAA; if(msaaVal!=0&&msaaVal!=2&&msaaVal!=4&&msaaVal!=8) msaaVal=0; SliderInt("MSAA Samples", msaaVal,0,8); int allowed[4]={0,2,4,8}; int best=0; int diff=999; for(int a:allowed){ int d=std::abs(a-msaaVal); if(d<diff){ diff=d; best=a; }} if(best!=MSAA){ MSAA=best; if(MSAA==0) glDisable(GL_MULTISAMPLE); else glEnable(GL_MULTISAMPLE); if(gUseDeferred) gDeferred.SetMSAASamples(MSAA); }
    Label("F1: Toggle GUI"); Label("Hierarchy: select row"); Label("Z=open X=close");
    buildHierarchyPanel();
    buildPerformancePanel(dt);
}

void SimpleGUI::flush(){ if(m_vertices.empty()) return; GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp); float L= (float)vp[0]; float T=(float)vp[1]; float W=(float)vp[2]; float H=(float)vp[3]; float R=L+W; float B=T+H; float proj[16]={ 2 /(R-L),0,0,0, 0,2/(T-B),0,0, 0,0,-1,0, (R+L)/(L-R),(T+B)/(B-T),0,1 }; GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST); GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE); GLboolean blendEnabled = glIsEnabled(GL_BLEND); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glUseProgram(m_shader); glUniform1i(m_uTex,0); glUniformMatrix4fv(m_uProj,1,GL_FALSE,proj); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,m_fontTex); glBindVertexArray(m_VAO); glBindBuffer(GL_ARRAY_BUFFER,m_VBO); glBufferSubData(GL_ARRAY_BUFFER,0,m_vertices.size()*sizeof(Vertex),m_vertices.data()); glDrawArrays(GL_TRIANGLES,0,(GLsizei)m_vertices.size()); glBindVertexArray(0); if(!blendEnabled) glDisable(GL_BLEND); if(cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE); if(depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); }
void SimpleGUI::Draw(float dt){ if(!m_initialized||!m_visible) return; buildPanels(dt); flush(); }
