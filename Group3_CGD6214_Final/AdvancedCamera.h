// AdvancedCamera.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>

enum class CameraMode {
    FIRST_PERSON,
    THIRD_PERSON,
    ORBITAL,
    CINEMATIC,
    FREE_FLY
};

enum class Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    ROLL_LEFT,
    ROLL_RIGHT
};

// Keyframe for cinematic camera paths
struct CameraKeyframe {
    glm::vec3 position;
    glm::vec3 target;
    float timestamp;
    float fov;

    CameraKeyframe(const glm::vec3& pos, const glm::vec3& tar, float time, float fieldOfView = 45.0f)
        : position(pos), target(tar), timestamp(time), fov(fieldOfView) {
    }
};

// Camera path for cinematic sequences
class CameraPath {
private:
    std::vector<CameraKeyframe> keyframes;
    bool looping;
    float totalDuration;

public:
    CameraPath(bool loop = false) : looping(loop), totalDuration(0.0f) {}

    void addKeyframe(const CameraKeyframe& keyframe) {
        keyframes.push_back(keyframe);
        if (keyframe.timestamp > totalDuration) {
            totalDuration = keyframe.timestamp;
        }
    }

    void clear() { keyframes.clear(); totalDuration = 0.0f; }

    CameraKeyframe interpolate(float time) const;
    float getDuration() const { return totalDuration; }
    bool isLooping() const { return looping; }
    void setLooping(bool loop) { looping = loop; }
};

class AdvancedCamera {
private:
    // Camera attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Camera mode and state
    CameraMode currentMode;
    glm::vec3 targetPosition; // For third-person and orbital modes
    float targetDistance;     // Distance from target

    // Euler angles
    float Yaw;
    float Pitch;
    float Roll;

    // Camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;
    float MinZoom, MaxZoom;

    // Collision detection
    bool collisionEnabled;
    float collisionRadius;
    std::vector<glm::vec3> obstacles;

    // Smooth movement and interpolation
    bool smoothMovement;
    float smoothFactor;
    glm::vec3 targetPositionSmooth;
    glm::vec3 targetFrontSmooth;

    // Cinematic camera
    std::unique_ptr<CameraPath> cinematicPath;
    float cinematicTime;
    bool cinematicActive;
    bool cinematicPaused;

    // Shake effect
    bool shakeEnabled;
    float shakeIntensity;
    float shakeDuration;
    float shakeTimer;
    glm::vec3 shakeOffset;

    // Auto-follow target (for third-person mode)
    glm::vec3* followTarget;
    glm::vec3 followOffset;

    // Field of view animation
    float targetFOV;
    bool animateFOV;

    // Internal methods
    void updateCameraVectors();
    void updateFirstPerson(float deltaTime);
    void updateThirdPerson(float deltaTime);
    void updateOrbital(float deltaTime);
    void updateCinematic(float deltaTime);
    void updateFreeFly(float deltaTime);
    void applyCollisionDetection();
    void updateShake(float deltaTime);
    void updateSmoothMovement(float deltaTime);

public:
    // Constructor
    AdvancedCamera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = -90.0f, float pitch = 0.0f);

    // Public interface
    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix(float aspect, float nearPlane = 0.1f, float farPlane = 1000.0f);

    // Camera mode management
    void SetMode(CameraMode mode);
    CameraMode GetMode() const { return currentMode; }

    // Movement processing
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);

    // Third-person and orbital camera
    void SetTarget(const glm::vec3& target);
    void SetFollowTarget(glm::vec3* target, const glm::vec3& offset = glm::vec3(0, 5, 10));
    void SetTargetDistance(float distance) { targetDistance = distance; }

    // Cinematic camera
    void StartCinematicSequence(std::unique_ptr<CameraPath> path);
    void PauseCinematic() { cinematicPaused = true; }
    void ResumeCinematic() { cinematicPaused = false; }
    void StopCinematic();
    bool IsCinematicActive() const { return cinematicActive; }

    // Camera effects
    void StartShake(float intensity, float duration);
    void SetSmoothMovement(bool enable, float factor = 5.0f);

    // Collision system
    void EnableCollision(bool enable, float radius = 1.0f);
    void AddObstacle(const glm::vec3& position);
    void ClearObstacles() { obstacles.clear(); }

    // Smooth transitions
    void TransitionToPosition(const glm::vec3& newPosition, float duration);
    void TransitionToTarget(const glm::vec3& newTarget, float duration);
    void AnimateFOV(float newFOV, float duration);

    // Update method (call every frame)
    void Update(float deltaTime);

    // Getters and setters
    glm::vec3 GetPosition() const { return Position; }
    glm::vec3 GetFront() const { return Front; }
    glm::vec3 GetUp() const { return Up; }
    glm::vec3 GetRight() const { return Right; }
    float GetZoom() const { return Zoom; }
    float GetYaw() const { return Yaw; }
    float GetPitch() const { return Pitch; }
    float GetRoll() const { return Roll; }

    void SetPosition(const glm::vec3& position) { Position = position; }
    void SetFront(const glm::vec3& front) { Front = front; updateCameraVectors(); }
    void SetZoom(float zoom) { Zoom = glm::clamp(zoom, MinZoom, MaxZoom); }
    void SetMovementSpeed(float speed) { MovementSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { MouseSensitivity = sensitivity; }

    // Utility functions
    glm::vec3 ScreenToWorldRay(float screenX, float screenY, float screenWidth, float screenHeight);
    bool IntersectRayPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
        const glm::vec3& planePoint, const glm::vec3& planeNormal,
        glm::vec3& intersection);
};