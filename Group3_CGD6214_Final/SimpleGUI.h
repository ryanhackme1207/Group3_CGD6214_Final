#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>
struct GLFWwindow; class SceneNode; class Camera; // forward declarations
class SimpleGUI {
public:
    static SimpleGUI& Instance();
    void Initialize(GLFWwindow* window);
    void BeginFrame();
    void Draw(float deltaTime);
    void ToggleVisible(){ m_visible = !m_visible; }
    bool IsVisible() const { return m_visible; }
    // Scene hierarchy integration
    void SetSceneRoot(SceneNode* root){ m_sceneRoot = root; m_openState[root]=true; }
    SceneNode* GetSelectedNode() const { return m_selectedNode; }
    void SetCamera(Camera* cam){ m_camera = cam; }
private:
    SimpleGUI()=default; ~SimpleGUI()=default; SimpleGUI(const SimpleGUI&)=delete; SimpleGUI& operator=(const SimpleGUI&)=delete;
    // basic widgets
    bool Button(const std::string& label, float w=0.0f); bool Toggle(const std::string& label, bool& value);
    bool SliderFloat(const std::string& label, float& value, float minV, float maxV, float step=0.0f);
    bool SliderInt(const std::string& label, int& value, int minV, int maxV);
    void Label(const std::string& text);
    // low-level draw
    void addQuad(float x,float y,float w,float h,uint32_t rgba,float u0,float v0,float u1,float v1);
    void addRect(float x,float y,float w,float h,uint32_t rgba);
    void addText(float x,float y,const std::string& text,uint32_t rgba=0xFFFFFFFF);
    void flush(); void buildPanels(float deltaTime); void buildFont(); void buildShaders();
    // panels
    void buildHierarchyPanel();
    void buildPerformancePanel(float deltaTime);
    void buildHelpPanel();
    // hierarchy helpers
    void drawNodeRecursive(SceneNode* node,int depth,float& y,float panelX,float panelW);
    void openAll(SceneNode* n); void closeAll(SceneNode* n);
    // performance helpers
    void recordFrameTime(float dt);
    // picking & interaction
    void handlePicking();
    void traversePick(SceneNode* n,const glm::vec3& ro,const glm::vec3& rd);
    void beginDrag(SceneNode* n,const glm::vec3& worldHit);
    void updateDrag(const glm::vec3& ro,const glm::vec3& rd);
    void endDrag();
    bool mouseOverAnyGUI() const; // exclude picking when hovering GUI
    // tooltip / context help
    void showTooltip();

    // state
    float m_cursorX=10.f,m_cursorY=10.f,m_panelWidth=260.f,m_lineHeight=16.f; GLFWwindow* m_window=nullptr; bool m_mouseDown=false,m_mousePressed=false; double m_mouseX=0.0,m_mouseY=0.0; bool m_initialized=false,m_visible=true; double m_timeAccum=0.0; int m_frameCount=0; float m_fps=0.f; unsigned int m_fontTex=0,m_VAO=0,m_VBO=0,m_shader=0; int m_uProj=-1,m_uTex=-1; struct Vertex{ float x,y,u,v; uint32_t color; }; std::vector<Vertex> m_vertices; bool regionHit(float x,float y,float w,float h) const;
    // hierarchy state
    SceneNode* m_sceneRoot=nullptr; SceneNode* m_selectedNode=nullptr; SceneNode* m_hoveredNode=nullptr; std::unordered_map<SceneNode*, bool> m_openState; bool m_prevZ=false; bool m_prevX=false; bool m_autoExpandedOnce=false; bool m_zPressedFrame=false; bool m_xPressedFrame=false;
    // keyboard navigation (new)
    std::vector<SceneNode*> m_visibleNodes; int m_navIndex=-1; bool m_prevDown=false; bool m_prevUp=false; bool m_prevEnter=false; bool m_downPressedFrame=false; bool m_upPressedFrame=false; bool m_enterPressedFrame=false;
    // performance history
    static const int FRAME_HISTORY = 120; float m_frameTimes[FRAME_HISTORY]={0}; int m_frameIndex=0; bool m_historyFilled=false; float m_minFrameMs=0.f; float m_maxFrameMs=0.f; float m_avgFrameMs=0.f;
    // picking
    Camera* m_camera=nullptr; SceneNode* m_pickCandidate=nullptr; float m_pickCandidateDist=1e9f; bool m_requestPick=false; bool m_dragging=false; glm::vec3 m_dragStartHit{0}; glm::vec3 m_dragStartNodePos{0};
    // help / shortcuts
    bool m_showHelp=true; std::string m_lastHover; double m_lastHoverTime=0.0; double m_timeNow=0.0; bool m_showContextTip=true;};
