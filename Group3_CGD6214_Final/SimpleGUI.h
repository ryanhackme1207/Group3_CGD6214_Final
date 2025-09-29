#pragma once
#include <vector>
#include <string>
#include <cstdint>
struct GLFWwindow;
class SimpleGUI {
public:
    static SimpleGUI& Instance();
    void Initialize(GLFWwindow* window);
    void BeginFrame();
    void Draw(float deltaTime);
    void ToggleVisible(){ m_visible = !m_visible; }
    bool IsVisible() const { return m_visible; }
private:
    SimpleGUI()=default; ~SimpleGUI()=default; SimpleGUI(const SimpleGUI&)=delete; SimpleGUI& operator=(const SimpleGUI&)=delete;
    bool Button(const std::string& label, float w=0.0f); bool Toggle(const std::string& label, bool& value);
    bool SliderFloat(const std::string& label, float& value, float minV, float maxV, float step=0.0f);
    bool SliderInt(const std::string& label, int& value, int minV, int maxV);
    void Label(const std::string& text);
    void addQuad(float x,float y,float w,float h,uint32_t rgba,float u0,float v0,float u1,float v1);
    void addRect(float x,float y,float w,float h,uint32_t rgba);
    void addText(float x,float y,const std::string& text,uint32_t rgba=0xFFFFFFFF);
    void flush(); void buildPanels(float deltaTime); void buildFont(); void buildShaders();
    float m_cursorX=10.f,m_cursorY=10.f,m_panelWidth=260.f,m_lineHeight=16.f; GLFWwindow* m_window=nullptr; bool m_mouseDown=false,m_mousePressed=false; double m_mouseX=0.0,m_mouseY=0.0; bool m_initialized=false,m_visible=true; double m_timeAccum=0.0; int m_frameCount=0; float m_fps=0.f; float m_panelStartY=0.f; unsigned int m_fontTex=0,m_VAO=0,m_VBO=0,m_shader=0; int m_uProj=-1,m_uTex=-1; struct Vertex{ float x,y,u,v; uint32_t color; }; std::vector<Vertex> m_vertices; bool regionHit(float x,float y,float w,float h) const;};
