#include "SimpleGUI.h"
#include "DeferredRenderer.h"
#include "SceneNode.h"
#include "Camera.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

// extern declarations
extern float timeOfDay;
extern bool useDirectionalLight;
extern int MSAA;
extern bool gUseDeferred;
extern DeferredRenderer gDeferred;

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
    if(!m_initialized) return; glfwGetCursorPos(m_window,&m_mouseX,&m_mouseY); int st=glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT); m_mousePressed=(st==GLFW_PRESS && !m_mouseDown); m_mouseDown=(st==GLFW_PRESS); m_vertices.clear();
    int zState=glfwGetKey(m_window,GLFW_KEY_Z); int xState=glfwGetKey(m_window,GLFW_KEY_X); m_zPressedFrame=(zState==GLFW_PRESS && !m_prevZ); m_xPressedFrame=(xState==GLFW_PRESS && !m_prevX); m_prevZ=(zState==GLFW_PRESS); m_prevX=(xState==GLFW_PRESS);
    int dState=glfwGetKey(m_window,GLFW_KEY_DOWN); int uState=glfwGetKey(m_window,GLFW_KEY_UP); int eState=glfwGetKey(m_window,GLFW_KEY_ENTER); if(eState==GLFW_KEY_UNKNOWN) eState=glfwGetKey(m_window,GLFW_KEY_KP_ENTER); m_downPressedFrame=(dState==GLFW_PRESS && !m_prevDown); m_upPressedFrame=(uState==GLFW_PRESS && !m_prevUp); m_enterPressedFrame=(eState==GLFW_PRESS && !m_prevEnter); m_prevDown=(dState==GLFW_PRESS); m_prevUp=(uState==GLFW_PRESS); m_prevEnter=(eState==GLFW_PRESS); m_hoveredNode=nullptr; // reset each frame; hierarchy build will set
}

void SimpleGUI::openAll(SceneNode* n){ if(!n) return; m_openState[n]=true; for(auto &c: n->GetChildren()) openAll(c.get()); }
void SimpleGUI::closeAll(SceneNode* n){ if(!n) return; m_openState[n]=false; for(auto &c: n->GetChildren()) closeAll(c.get()); }

static void CollectVisible(SceneNode* n, std::unordered_map<SceneNode*,bool>& openMap, std::vector<SceneNode*>& out){ if(!n) return; out.push_back(n); if(openMap[n]) for(auto &c: n->GetChildren()) CollectVisible(c.get(),openMap,out); }

void SimpleGUI::drawNodeRecursive(SceneNode* node,int depth,float& y,float panelX,float panelW){
    if(!node) return; float rowH=16.f; float rowY=y; float labelX=panelX+10.f+depth*14.f; bool hasChildren=!node->GetChildren().empty(); bool open=m_openState[node]; bool hover=regionHit(panelX+8.f,rowY,panelW-16.f,rowH); if(hover) m_hoveredNode=node; bool navHighlight=(m_navIndex>=0 && m_navIndex < (int)m_visibleNodes.size() && m_visibleNodes[m_navIndex]==node); uint32_t baseCol=0xFF2A2A60; if(navHighlight) baseCol=0xFF3A3A80; if(m_selectedNode==node) baseCol=0xFF5560AA; if(hover && node!=m_selectedNode) baseCol=0xFF444488; addRect(panelX+8.f,rowY,panelW-16.f,rowH,baseCol); if(hasChildren){ uint32_t ic=open?0xFF55CC55:0xFFCC5555; addRect(panelX+12.f+depth*14.f,rowY+4.f,8.f,8.f,ic);} addText(labelX + (hasChildren?14.f:0.f),rowY+4.f,node->name.empty()?"(unnamed)":node->name.c_str(),0xFFE0E0E0); if(hover && m_mousePressed){ m_selectedNode=node; m_navIndex=-1; if(hasChildren){ float iconX=panelX+12.f+depth*14.f; if(regionHit(iconX,rowY+4.f,8.f,8.f)) m_openState[node]=!open; }} if((navHighlight||m_selectedNode==node)&&hasChildren){ if(m_zPressedFrame){ m_openState[node]=true; open=true;} if(m_xPressedFrame){ m_openState[node]=false; open=false;} }
    // inline flashing border for selected
    if(m_selectedNode==node){ float t=fmodf((float)glfwGetTime()*2.0f,1.0f); uint32_t a=(uint32_t)((0.4f+0.6f*fabsf(t-0.5f)*2.f)*255.f); uint32_t col=0x0088CCFF | (a<<24); addRect(panelX+8.f,rowY,panelW-16.f,1,col); addRect(panelX+8.f,rowY+rowH-1,panelW-16.f,1,col); addRect(panelX+8.f,rowY,1,rowH,col); addRect(panelX+8.f+panelW-16.f-1,rowY,1,rowH,col); }
    y+=rowH+2.f; if(open){ for(auto &c: node->GetChildren()) drawNodeRecursive(c.get(),depth+1,y,panelX,panelW); }
}

void SimpleGUI::buildHierarchyPanel(){ if(!m_sceneRoot) return; if(!m_autoExpandedOnce){ openAll(m_sceneRoot); m_autoExpandedOnce=true; } if(!m_selectedNode) m_selectedNode=m_sceneRoot; m_visibleNodes.clear(); CollectVisible(m_sceneRoot,m_openState,m_visibleNodes); if(m_navIndex<0 || m_navIndex>=(int)m_visibleNodes.size()){ for(size_t i=0;i<m_visibleNodes.size();++i) if(m_visibleNodes[i]==m_selectedNode){ m_navIndex=(int)i; break; } } if(m_downPressedFrame && !m_visibleNodes.empty()) m_navIndex=std::min((int)m_visibleNodes.size()-1,m_navIndex+1); if(m_upPressedFrame && !m_visibleNodes.empty()) m_navIndex=std::max(0,m_navIndex-1); if(m_enterPressedFrame && m_navIndex>=0 && m_navIndex<(int)m_visibleNodes.size()){ SceneNode* target=m_visibleNodes[m_navIndex]; if(target){ if(m_selectedNode==target){ if(!target->GetChildren().empty()) m_openState[target]=!m_openState[target]; } else m_selectedNode=target; } }
    float startX=m_panelWidth+30.f; float panelW=m_panelWidth; float y=15.f; float panelH=360.f; addRect(startX,5,panelW+10,panelH+140,0xAA101018); addText(startX+15,10,"Scene Hierarchy (Up/Down, Enter, Z/X)",0xFFFFFFAA); y+=24.f; drawNodeRecursive(m_sceneRoot,0,y,startX,panelW); if(m_selectedNode){ addRect(startX+8.f,panelH+20,panelW-16.f,50.f,0x55202040); addText(startX+14.f,panelH+24.f,"Selected:",0xFFAAAAFF); addText(startX+14.f+9*8.f,panelH+24.f,m_selectedNode->name.empty()?"(unnamed)":m_selectedNode->name.c_str(),0xFFFFFFFF); }
    if(m_hoveredNode && m_hoveredNode!=m_selectedNode){ addText(startX+15.f,panelH+82.f,(std::string)"Hover: "+(m_hoveredNode->name.empty()?"(unnamed)":m_hoveredNode->name),0xFFCCCC99); }
}

void SimpleGUI::buildHelpPanel(){ float startX=m_panelWidth + 30.f + m_panelWidth + 40.f; float y=200.f; float w=m_panelWidth+40.f; float h=260.f; addRect(startX,y,w+10.f,h,0xAA101018); addText(startX+15,y+6,"Help / Shortcuts",0xFFFFFFAA); y+=24.f; auto line=[&](const char* t){ addText(startX+15,y,t,0xFFE0E0E0); y+=14.f; }; line("Mouse click: select"); line("Enter: select/toggle"); line("Z/X: open/close"); line("Shift+Click world: drag"); line("F1: toggle GUI"); line("F6: deferred"); line("M: cycle MSAA"); line("B: bloom toggle"); line("N: bloom intensity"); line("J/K: HDR exposure"); line("Esc: quit"); if(m_dragging) addText(startX+15,y+10,"[Dragging] release",0xFFFFAA55); }
void SimpleGUI::buildPanels(float dt){ addRect(5,5,m_panelWidth+10,500,0xAA101018); m_cursorX=15; m_cursorY=15; m_timeAccum+=dt; ++m_frameCount; if(m_timeAccum>=0.5){ m_fps=(float)(m_frameCount/m_timeAccum); m_timeAccum=0; m_frameCount=0;} char s[64]; std::snprintf(s,64,"FPS: %.1f",m_fps); Label(s); bool prev=gUseDeferred; Toggle("Deferred Rendering", gUseDeferred); if(gUseDeferred && !prev){ GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp); gDeferred.Initialize(vp[2],vp[3]); } if(gUseDeferred){ float exp=gDeferred.GetExposure(); SliderFloat("HDR Exposure",exp,0.1f,5.f,0.05f); gDeferred.SetExposure(exp); bool bloom=gDeferred.IsBloomEnabled(); Toggle("Bloom",bloom); gDeferred.SetBloomEnabled(bloom); float bi=gDeferred.GetBloomIntensity(); SliderFloat("Bloom Intensity",bi,0.f,5.f,0.05f); gDeferred.SetBloomIntensity(bi); } SliderFloat("Time Of Day", timeOfDay,0.f,24.f,0.01f); Toggle("Directional Light", useDirectionalLight); int msaaVal=MSAA; if(msaaVal!=0&&msaaVal!=2&&msaaVal!=4&&msaaVal!=8) msaaVal=0; SliderInt("MSAA Samples", msaaVal,0,8); int allowed[4]={0,2,4,8}; int best=0; int diff=999; for(int a:allowed){ int d=std::abs(a-msaaVal); if(d<diff){ diff=d; best=a; }} if(best!=MSAA){ MSAA=best; if(MSAA==0) glDisable(GL_MULTISAMPLE); else glEnable(GL_MULTISAMPLE); if(gUseDeferred) gDeferred.SetMSAASamples(MSAA); } Label("Hierarchy: Mouse / Keys"); buildHierarchyPanel(); buildPerformancePanel(dt); buildHelpPanel(); handlePicking(); }

void SimpleGUI::flush(){
    if(m_vertices.empty()) return;
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp);
    float L=(float)vp[0]; float T=(float)vp[1]; float W=(float)vp[2]; float H=(float)vp[3]; float R=L+W; float B=T+H;
    // column-major orthographic projection
    float proj[16]={ 2.f/(R-L),0,0,0, 0,2.f/(T-B),0,0, 0,0,-1,0, (R+L)/(L-R),(T+B)/(B-T),0,1 };
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
    glUseProgram(m_shader);
    glUniform1i(m_uTex,0);
    glUniformMatrix4fv(m_uProj,1,GL_FALSE,proj);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,m_fontTex);
    glBindVertexArray(m_VAO); glBindBuffer(GL_ARRAY_BUFFER,m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER,0,(GLsizeiptr)(m_vertices.size()*sizeof(Vertex)),m_vertices.data());
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)m_vertices.size());
    glBindVertexArray(0);
    // restore state
    if(!blendEnabled) glDisable(GL_BLEND);
    if(cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if(depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}

void SimpleGUI::Draw(float dt){
    if(!m_initialized || !m_visible) return;
    buildPanels(dt);
    flush();
}

// --- RESTORED / RE-IMPLEMENTED INTERNAL WIDGET & UTILITY FUNCTIONS ---

bool SimpleGUI::regionHit(float x,float y,float w,float h) const { return m_mouseX>=x && m_mouseX<=x+w && m_mouseY>=y && m_mouseY<=y+h; }

void SimpleGUI::addQuad(float x,float y,float w,float h,uint32_t rgba,float u0,float v0,float u1,float v1){ Vertex v; v.color=rgba; v.x=x; v.y=y; v.u=u0; v.v=v0; m_vertices.push_back(v); v.x=x+w; v.y=y; v.u=u1; v.v=v0; m_vertices.push_back(v); v.x=x+w; v.y=y+h; v.u=u1; v.v=v1; m_vertices.push_back(v); v.x=x; v.y=y; v.u=u0; v.v=v0; m_vertices.push_back(v); v.x=x+w; v.y=y+h; v.u=u1; v.v=v1; m_vertices.push_back(v); v.x=x; v.y=y+h; v.u=u0; v.v=v1; m_vertices.push_back(v); }
void SimpleGUI::addRect(float x,float y,float w,float h,uint32_t rgba){ addQuad(x,y,w,h,rgba,0,0,0,0); }

void SimpleGUI::addText(float x,float y,const std::string& text,uint32_t rgba){ float gw=8.f; float gu=1.f/128.f; for(size_t i=0;i<text.size();++i){ unsigned char c=(unsigned char)text[i]; if(c>=128) continue; float u0=c*gu; float u1=u0+gu; addQuad(x+i*gw,y,gw,8.f,rgba,u0,0,u1,1); } }

void SimpleGUI::Label(const std::string& t){ addText(m_cursorX,m_cursorY,t,0xFFFFFFFF); m_cursorY+=m_lineHeight; }

bool SimpleGUI::Button(const std::string& label,float w){ float bw=(w>0?w:m_panelWidth-20.f); float h=18.f; float x=m_cursorX; float y=m_cursorY; bool hov=regionHit(x,y,bw,h); addRect(x,y,bw,h, hov?0xFF4A4A90:0xFF2A2A60); addText(x+6,y+5,label); bool clk=hov&&m_mousePressed; m_cursorY+=h+4.f; return clk; }

bool SimpleGUI::Toggle(const std::string& label,bool& value){ float h=16.f; float x=m_cursorX; float y=m_cursorY; float box=14.f; bool hov=regionHit(x,y,box,h); uint32_t col=value?0xFF2BAA55:0xFF666666; if(hov) col^=0x00222222; addRect(x,y,box,box,col); if(value) addText(x+3,y+3,"X"); addText(x+box+6,y+2,label); if(hov&&m_mousePressed) value=!value; m_cursorY+=h+6.f; return value; }

bool SimpleGUI::SliderFloat(const std::string& label,float& value,float minV,float maxV,float step){ addText(m_cursorX,m_cursorY,label,0xFFE0E0E0); m_cursorY+=12.f; float x=m_cursorX; float y=m_cursorY; float w=m_panelWidth-20.f; float h=14.f; addRect(x,y,w,h,0xFF202020); float norm=(value-minV)/(maxV-minV); norm=std::max(0.f,std::min(1.f,norm)); float knobW=10.f; float kx=x+norm*(w-knobW); bool hov=regionHit(x,y,w,h); if(hov && m_mouseDown){ double mx=m_mouseX; if(mx<x) mx=x; if(mx>x+w) mx=x+w; norm=float((mx-x)/w); value=minV+norm*(maxV-minV); if(step>0){ float stp=roundf((value-minV)/step); value=minV+stp*step; value=std::max(minV,std::min(maxV,value)); }} addRect(kx,y,knobW,h, hov?0xFF8888CC:0xFF666699); char buf[48]; std::snprintf(buf,48,"%.2f",value); addText(x+w-(float)strlen(buf)*8.f-2.f,y-10.f,buf,0xFFB0FFB0); m_cursorY+=h+8.f; return hov; }

bool SimpleGUI::SliderInt(const std::string& label,int& value,int minV,int maxV){ float f=(float)value; bool ch=SliderFloat(label,f,(float)minV,(float)maxV,1.f); int nv=(int)roundf(f); if(nv!=value){ value=nv; ch=true;} return ch; }

// --- PERFORMANCE PANEL SUPPORT ---
void SimpleGUI::recordFrameTime(float dt){ float ms=dt*1000.0f; m_frameTimes[m_frameIndex++]=ms; if(m_frameIndex>=FRAME_HISTORY){ m_frameIndex=0; m_historyFilled=true; } int count=m_historyFilled?FRAME_HISTORY:m_frameIndex; if(!count) return; m_minFrameMs=m_frameTimes[0]; m_maxFrameMs=m_frameTimes[0]; double sum=0; for(int i=0;i<count;++i){ float v=m_frameTimes[i]; if(v<m_minFrameMs) m_minFrameMs=v; if(v>m_maxFrameMs) m_maxFrameMs=v; sum+=v; } m_avgFrameMs=(float)(sum/count); }

void SimpleGUI::buildPerformancePanel(float dt){ recordFrameTime(dt); float startX = m_panelWidth + 30.f + m_panelWidth + 40.f; float panelW = m_panelWidth + 40.f; float panelH = 180.f; addRect(startX,5,panelW+10,panelH+140,0xAA101018); addText(startX+15,10,"Performance",0xFFFFFFAA); char line[128]; std::snprintf(line,128,"Avg: %.2f ms (%.0f FPS)", m_avgFrameMs,(m_avgFrameMs>0?1000.0f/m_avgFrameMs:0.f)); addText(startX+15,30,line,0xFFE0E0E0); std::snprintf(line,128,"Min: %.2f ms  Max: %.2f ms", m_minFrameMs, m_maxFrameMs); addText(startX+15,46,line,0xFFB0B0B0); float graphX=startX+15.f, graphY=66.f, graphW=panelW-30.f, graphH=90.f; addRect(graphX,graphY,graphW,graphH,0xFF1A1A30); int count=m_historyFilled?FRAME_HISTORY:m_frameIndex; if(count>1){ float maxV=m_maxFrameMs; if(maxV<16.f) maxV=16.f; float barW=graphW/(float)FRAME_HISTORY; for(int i=0;i<count;++i){ int idx=(m_historyFilled?(m_frameIndex+i)%FRAME_HISTORY:i); float v=m_frameTimes[idx]; float norm=v/maxV; if(norm>1) norm=1; float h=(graphH-4.f)*norm; float bx=graphX+i*barW; float by=graphY+graphH-2.f-h; uint32_t col=(v>33.0f)?0xFFAA3333:((v>22.f)?0xFFAA8833:0xFF33AA55); addRect(bx,by,barW-1.f,h,col);} float ref60Y = graphY + graphH - 2.f - (16.6f / maxV) * (graphH-4.f); float ref30Y = graphY + graphH - 2.f - (33.3f / maxV) * (graphH-4.f); addRect(graphX,ref60Y,graphW,1.f,0x40FFFFFF); addRect(graphX,ref30Y,graphW,1.f,0x40FFAA66); addText(graphX+graphW-50.f,ref60Y-8.f,"60fps",0x80FFFFFF); addText(graphX+graphW-50.f,ref30Y-8.f,"30fps",0x80FFCC99); } }

// --- PICKING / DRAGGING (copied simplified earlier logic) ---

bool SimpleGUI::mouseOverAnyGUI() const { if(!m_visible) return false; double mx=m_mouseX, my=m_mouseY; if(mx < m_panelWidth+20 && my < 520) return true; if(mx > m_panelWidth+30 && mx < m_panelWidth*2+60 && my < 420) return true; if(mx > m_panelWidth*2+100) return true; return false; }

static bool intersectAABB(const glm::vec3& ro,const glm::vec3& rd,const glm::vec3& center,const glm::vec3& half,float& t){ glm::vec3 minB=center-half; glm::vec3 maxB=center+half; float tmin=0.f; float tmax=1e9f; for(int i=0;i<3;++i){ if(fabs(rd[i])<1e-6f){ if(ro[i]<minB[i]||ro[i]>maxB[i]) return false; } else { float inv=1.f/rd[i]; float t1=(minB[i]-ro[i])*inv; float t2=(maxB[i]-ro[i])*inv; if(t1>t2) std::swap(t1,t2); tmin = t1>tmin? t1 : tmin; tmax = t2<tmax? t2 : tmax; if(tmin>tmax) return false; } } t=tmin; return true; }

void SimpleGUI::handlePicking(){ if(!m_camera||!m_sceneRoot) return; if(mouseOverAnyGUI()) return; if(m_mousePressed){ m_requestPick=true; m_pickCandidate=nullptr; m_pickCandidateDist=1e9f; } if(!m_requestPick) return; int w,h; glfwGetWindowSize(m_window,&w,&h); float nx=(float)((m_mouseX/(double)w)*2.0-1.0); float ny=(float)(1.0-(m_mouseY/(double)h)*2.0); glm::mat4 proj=m_camera->GetProjectionMatrix((float)w/(float)h); glm::mat4 view=m_camera->GetViewMatrix(); glm::mat4 inv=glm::inverse(proj*view); glm::vec4 pn=inv*glm::vec4(nx,ny,-1,1); pn/=pn.w; glm::vec4 pf=inv*glm::vec4(nx,ny,1,1); pf/=pf.w; glm::vec3 ro=glm::vec3(pn); glm::vec3 rd=glm::normalize(glm::vec3(pf-pn)); traversePick(m_sceneRoot,ro,rd); if(!m_mouseDown && m_requestPick){ if(m_pickCandidate){ m_selectedNode=m_pickCandidate; int shift=glfwGetKey(m_window,GLFW_KEY_LEFT_SHIFT); if(shift==GLFW_PRESS) beginDrag(m_selectedNode,m_dragStartHit); } m_requestPick=false; } if(m_dragging){ updateDrag(ro,rd); if(!m_mouseDown) endDrag(); } }

void SimpleGUI::traversePick(SceneNode* n,const glm::vec3& ro,const glm::vec3& rd){ if(!n) return; glm::mat4 wt=n->GetWorldTransform(); glm::vec3 pos=glm::vec3(wt[3]); glm::vec3 half=glm::vec3(glm::length(glm::vec3(wt[0]))*0.5f, glm::length(glm::vec3(wt[1]))*0.5f, glm::length(glm::vec3(wt[2]))*0.5f); if(half.x<0.2f) half=glm::max(half,glm::vec3(0.5f)); float t; if(intersectAABB(ro,rd,pos,half,t)){ if(t<m_pickCandidateDist){ m_pickCandidateDist=t; m_pickCandidate=n; m_dragStartHit=ro+rd*t; m_dragStartNodePos=pos; } } for(auto &c : n->GetChildren()) traversePick(c.get(),ro,rd); }
void SimpleGUI::beginDrag(SceneNode* n,const glm::vec3& worldHit){ if(!n) return; m_dragging=true; m_dragStartHit=worldHit; m_dragStartNodePos=glm::vec3(n->GetWorldTransform()[3]); }
void SimpleGUI::updateDrag(const glm::vec3& ro,const glm::vec3& rd){ if(!m_dragging||!m_selectedNode) return; float planeY=m_dragStartHit.y; if(fabs(rd.y)<1e-5f) return; float t=(planeY-ro.y)/rd.y; if(t<0) return; glm::vec3 hit=ro+rd*t; glm::vec3 delta=hit-m_dragStartHit; glm::vec3 newPos=m_dragStartNodePos+glm::vec3(delta.x,0,delta.z); glm::mat4 lt=m_selectedNode->GetLocalTransform(); SceneNode* parent=m_selectedNode->GetParent(); if(parent){ glm::mat4 parentW=parent->GetWorldTransform(); glm::mat4 parentInv=glm::inverse(parentW); glm::mat4 newWorld=glm::mat4(1); newWorld[3]=glm::vec4(newPos,1); lt=parentInv*newWorld; } else { lt[3]=glm::vec4(newPos,1); } m_selectedNode->SetLocalTransform(lt); }
void SimpleGUI::endDrag(){ m_dragging=false; }
