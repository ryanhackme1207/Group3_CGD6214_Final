#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 10.0f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;
const float MIN_ZOOM = 1.0f;
const float MAX_ZOOM = 75.0f;

class Camera {
public:
    // Camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    
    // Camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;
    
    // Euler Angles
    float Yaw;
    float Pitch;
    
    // Smooth movement
    bool SmoothMovement;
    float Smoothness;
    glm::vec3 TargetPosition;
    glm::vec3 Velocity;
    
    // Collision boundaries
    float BoundaryMinX, BoundaryMaxX;
    float BoundaryMinZ, BoundaryMaxZ;
    float BoundaryMinY, BoundaryMaxY;
    float CollisionRadius;
    
    // Camera modes
    enum CameraMode {
        FREE_FLY,
        FIRST_PERSON,
        ORBITAL,
        CINEMATIC
    };
    CameraMode Mode;
    
    // Orbital camera specific
    glm::vec3 OrbitTarget;
    float OrbitDistance;
    float OrbitSpeed;
    
    // Constructors
    Camera(glm::vec3 position = glm::vec3(0.0f, 5.0f, 10.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = YAW, float pitch = PITCH);
    Camera(float posX, float posY, float posZ,
           float upX, float upY, float upZ,
           float yaw, float pitch);
    
    // Core functions
    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix(float aspectRatio, float nearPlane = 0.1f, float farPlane = 1000.0f);
    
    // Input processing
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);
    
    // Boundary and collision system
    void SetBoundaries(float minX, float maxX, float minZ, float maxZ,
                      float minY = 1.0f, float maxY = 50.0f);
    void SetCollisionRadius(float radius);
    bool CheckCollisionWithBuilding(glm::vec3 newPosition, const std::vector<glm::vec3>& buildingPositions,
                                   const std::vector<glm::vec3>& buildingSizes);
    void ApplyBoundaries();
    
    // Smooth movement system
    void EnableSmoothMovement(bool enable, float smoothness = 5.0f);
    void UpdateSmoothMovement(float deltaTime);
    void SetTargetPosition(glm::vec3 target);
    
    // Camera modes
    void SetCameraMode(CameraMode mode);
    void UpdateOrbitalCamera(float deltaTime);
    void SetOrbitTarget(glm::vec3 target, float distance = 15.0f);
    
    // Utility functions
    void ResetToDefault();
    glm::vec3 GetPosition() const { return Position; }
    glm::vec3 GetFront() const { return Front; }
    float GetZoom() const { return Zoom; }
    
    // Cinematic camera functions
    void SmoothTransitionTo(glm::vec3 newPosition, glm::vec3 newTarget, float duration);
    bool IsTransitioning() const { return isTransitioning; }
    void UpdateTransition(float deltaTime);

private:
    // Internal state for smooth transitions
    bool isTransitioning;
    float transitionTime;
    float transitionDuration;
    glm::vec3 startPosition;
    glm::vec3 endPosition;
    glm::vec3 startTarget;
    glm::vec3 endTarget;
    
    // Helper functions
    void updateCameraVectors();
    float lerp(float a, float b, float t);
    glm::vec3 lerp(const glm::vec3& a, const glm::vec3& b, float t);
    float smoothstep(float t);
};