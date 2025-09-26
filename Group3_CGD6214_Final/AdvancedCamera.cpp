// AdvancedCamera.cpp Implementation
#include "AdvancedCamera.h"
#include <algorithm>
#include <cmath>

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 15.0f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;

AdvancedCamera::AdvancedCamera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY),
    Zoom(ZOOM), MinZoom(1.0f), MaxZoom(120.0f), Position(position), WorldUp(up),
    Yaw(yaw), Pitch(pitch), Roll(0.0f), currentMode(CameraMode::FREE_FLY),
    targetDistance(10.0f), collisionEnabled(false), collisionRadius(1.0f),
    smoothMovement(false), smoothFactor(5.0f), cinematicActive(false), cinematicPaused(false),
    cinematicTime(0.0f), shakeEnabled(false), shakeIntensity(0.0f), shakeDuration(0.0f),
    shakeTimer(0.0f), followTarget(nullptr), targetFOV(ZOOM), animateFOV(false) {

    targetPosition = position;
    targetPositionSmooth = position;
    targetFrontSmooth = Front;
    followOffset = glm::vec3(0, 5, 10);
    shakeOffset = glm::vec3(0.0f);

    updateCameraVectors();
}

void AdvancedCamera::updateCameraVectors() {
    // Calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    // Calculate Right and Up vectors
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));

    // Apply roll rotation
    if (abs(Roll) > 0.01f) {
        glm::mat4 rollMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(Roll), Front);
        Up = glm::vec3(rollMatrix * glm::vec4(Up, 0.0f));
        Right = glm::normalize(glm::cross(Front, Up));
    }
}

glm::mat4 AdvancedCamera::GetViewMatrix() {
    glm::vec3 finalPosition = Position + shakeOffset;
    return glm::lookAt(finalPosition, finalPosition + Front, Up);
}

glm::mat4 AdvancedCamera::GetProjectionMatrix(float aspect, float nearPlane, float farPlane) {
    return glm::perspective(glm::radians(Zoom), aspect, nearPlane, farPlane);
}

void AdvancedCamera::SetMode(CameraMode mode) {
    if (currentMode != mode) {
        currentMode = mode;

        // Mode-specific initialization
        switch (mode) {
        case CameraMode::THIRD_PERSON:
            if (targetDistance < 1.0f) targetDistance = 10.0f;
            break;
        case CameraMode::ORBITAL:
            if (targetDistance < 1.0f) targetDistance = 15.0f;
            break;
        case CameraMode::CINEMATIC:
            StopCinematic(); // Stop any existing cinematic
            break;
        default:
            break;
        }
    }
}

void AdvancedCamera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    if (cinematicActive && !cinematicPaused) return; // Don't allow input during cinematics

    float velocity = MovementSpeed * deltaTime;

    switch (direction) {
    case Camera_Movement::FORWARD:
        Position += Front * velocity;
        break;
    case Camera_Movement::BACKWARD:
        Position -= Front * velocity;
        break;
    case Camera_Movement::LEFT:
        Position -= Right * velocity;
        break;
    case Camera_Movement::RIGHT:
        Position += Right * velocity;
        break;
    case Camera_Movement::UP:
        Position += WorldUp * velocity;
        break;
    case Camera_Movement::DOWN:
        Position -= WorldUp * velocity;
        break;
    case Camera_Movement::ROLL_LEFT:
        Roll -= 45.0f * deltaTime;
        break;
    case Camera_Movement::ROLL_RIGHT:
        Roll += 45.0f * deltaTime;
        break;
    }

    // Apply collision detection
    if (collisionEnabled) {
        applyCollisionDetection();
    }
}

void AdvancedCamera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    if (cinematicActive && !cinematicPaused) return;

    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    if (currentMode == CameraMode::ORBITAL) {
        // Orbital camera rotates around target
        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            Pitch = glm::clamp(Pitch, -89.0f, 89.0f);
        }

        updateOrbital(0.0f);
    }
    else {
        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            Pitch = glm::clamp(Pitch, -89.0f, 89.0f);
        }

        updateCameraVectors();
    }
}

void AdvancedCamera::ProcessMouseScroll(float yoffset) {
    if (currentMode == CameraMode::THIRD_PERSON || currentMode == CameraMode::ORBITAL) {
        targetDistance -= yoffset * 2.0f;
        targetDistance = glm::clamp(targetDistance, 2.0f, 50.0f);
    }
    else {
        Zoom -= yoffset * 2.0f;
        Zoom = glm::clamp(Zoom, MinZoom, MaxZoom);
    }
}

void AdvancedCamera::SetTarget(const glm::vec3& target) {
    targetPosition = target;
}

void AdvancedCamera::SetFollowTarget(glm::vec3* target, const glm::vec3& offset) {
    followTarget = target;
    followOffset = offset;
}

void AdvancedCamera::Update(float deltaTime) {
    // Update camera based on current mode
    switch (currentMode) {
    case CameraMode::FIRST_PERSON:
        updateFirstPerson(deltaTime);
        break;
    case CameraMode::THIRD_PERSON:
        updateThirdPerson(deltaTime);
        break;
    case CameraMode::ORBITAL:
        updateOrbital(deltaTime);
        break;
    case CameraMode::CINEMATIC:
        updateCinematic(deltaTime);
        break;
    case CameraMode::FREE_FLY:
        updateFreeFly(deltaTime);
        break;
    }

    // Update effects
    updateShake(deltaTime);
    updateSmoothMovement(deltaTime);

    // Update FOV animation
    if (animateFOV) {
        float fovDiff = targetFOV - Zoom;
        if (abs(fovDiff) > 0.1f) {
            Zoom += fovDiff * 3.0f * deltaTime;
        }
        else {
            Zoom = targetFOV;
            animateFOV = false;
        }
    }

    // Keep roll in reasonable bounds
    while (Roll > 360.0f) Roll -= 360.0f;
    while (Roll < -360.0f) Roll += 360.0f;
}

void AdvancedCamera::updateFirstPerson(float deltaTime) {
    // First person is just the basic camera with potential head bob
    updateCameraVectors();
}

void AdvancedCamera::updateThirdPerson(float deltaTime) {
    if (followTarget) {
        targetPosition = *followTarget;
    }

    // Calculate desired position behind target
    glm::vec3 desiredPosition = targetPosition - Front * targetDistance + glm::vec3(0, followOffset.y, 0);

    if (smoothMovement) {
        Position = glm::mix(Position, desiredPosition, smoothFactor * deltaTime);
    }
    else {
        Position = desiredPosition;
    }

    // Always look at target
    Front = glm::normalize(targetPosition - Position);
    updateCameraVectors();
}

void AdvancedCamera::updateOrbital(float deltaTime) {
    // Calculate position based on spherical coordinates around target
    float x = targetDistance * cos(glm::radians(Pitch)) * cos(glm::radians(Yaw));
    float y = targetDistance * sin(glm::radians(Pitch));
    float z = targetDistance * cos(glm::radians(Pitch)) * sin(glm::radians(Yaw));

    glm::vec3 desiredPosition = targetPosition + glm::vec3(x, y, z);

    if (smoothMovement) {
        Position = glm::mix(Position, desiredPosition, smoothFactor * deltaTime);
    }
    else {
        Position = desiredPosition;
    }

    // Always look at target
    Front = glm::normalize(targetPosition - Position);
    updateCameraVectors();
}

void AdvancedCamera::updateCinematic(float deltaTime) {
    if (!cinematicActive || !cinematicPath || cinematicPaused) return;

    cinematicTime += deltaTime;

    if (cinematicTime > cinematicPath->getDuration()) {
        if (cinematicPath->isLooping()) {
            cinematicTime = 0.0f;
        }
        else {
            StopCinematic();
            return;
        }
    }

    CameraKeyframe frame = cinematicPath->interpolate(cinematicTime);
    Position = frame.position;
    Front = glm::normalize(frame.target - frame.position);
    Zoom = frame.fov;

    updateCameraVectors();
}

void AdvancedCamera::updateFreeFly(float deltaTime) {
    // Free fly mode - basic camera movement
    updateCameraVectors();
}

void AdvancedCamera::StartCinematicSequence(std::unique_ptr<CameraPath> path) {
    cinematicPath = std::move(path);
    cinematicActive = true;
    cinematicPaused = false;
    cinematicTime = 0.0f;
}

void AdvancedCamera::StopCinematic() {
    cinematicActive = false;
    cinematicPaused = false;
    cinematicTime = 0.0f;
    cinematicPath.reset();
}

void AdvancedCamera::StartShake(float intensity, float duration) {
    shakeEnabled = true;
    shakeIntensity = intensity;
    shakeDuration = duration;
    shakeTimer = 0.0f;
}

void AdvancedCamera::updateShake(float deltaTime) {
    if (!shakeEnabled) return;

    shakeTimer += deltaTime;

    if (shakeTimer >= shakeDuration) {
        shakeEnabled = false;
        shakeOffset = glm::vec3(0.0f);
        return;
    }

    float shakeAmount = shakeIntensity * (1.0f - shakeTimer / shakeDuration);
    shakeOffset.x = ((rand() % 1000) / 1000.0f - 0.5f) * 2.0f * shakeAmount;
    shakeOffset.y = ((rand() % 1000) / 1000.0f - 0.5f) * 2.0f * shakeAmount;
    shakeOffset.z = ((rand() % 1000) / 1000.0f - 0.5f) * 2.0f * shakeAmount;
}

void AdvancedCamera::updateSmoothMovement(float deltaTime) {
    if (!smoothMovement) return;

    // Smooth position interpolation
    glm::vec3 positionDiff = targetPositionSmooth - Position;
    if (glm::length(positionDiff) > 0.01f) {
        Position = glm::mix(Position, targetPositionSmooth, smoothFactor * deltaTime);
    }

    // Smooth front direction interpolation
    glm::vec3 frontDiff = targetFrontSmooth - Front;
    if (glm::length(frontDiff) > 0.01f) {
        Front = glm::normalize(glm::mix(Front, targetFrontSmooth, smoothFactor * deltaTime));
        updateCameraVectors();
    }
}

void AdvancedCamera::SetSmoothMovement(bool enable, float factor) {
    smoothMovement = enable;
    smoothFactor = factor;
}

void AdvancedCamera::EnableCollision(bool enable, float radius) {
    collisionEnabled = enable;
    collisionRadius = radius;
}

void AdvancedCamera::AddObstacle(const glm::vec3& position) {
    obstacles.push_back(position);
}

void AdvancedCamera::applyCollisionDetection() {
    for (const auto& obstacle : obstacles) {
        float distance = glm::length(Position - obstacle);
        if (distance < collisionRadius) {
            glm::vec3 pushDirection = glm::normalize(Position - obstacle);
            Position = obstacle + pushDirection * collisionRadius;
        }
    }
}

void AdvancedCamera::AnimateFOV(float newFOV, float duration) {
    targetFOV = newFOV;
    animateFOV = true;
}

glm::vec3 AdvancedCamera::ScreenToWorldRay(float screenX, float screenY, float screenWidth, float screenHeight) {
    // Convert screen coordinates to normalized device coordinates
    float x = (2.0f * screenX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * screenY) / screenHeight;

    // Create ray in clip space
    glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);

    // Convert to eye space
    glm::mat4 projMatrix = GetProjectionMatrix(screenWidth / screenHeight);
    glm::vec4 rayEye = glm::inverse(projMatrix) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    // Convert to world space
    glm::mat4 viewMatrix = GetViewMatrix();
    glm::vec3 rayWorld = glm::vec3(glm::inverse(viewMatrix) * rayEye);
    return glm::normalize(rayWorld);
}

bool AdvancedCamera::IntersectRayPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& planePoint, const glm::vec3& planeNormal,
    glm::vec3& intersection) {
    float denom = glm::dot(planeNormal, rayDir);
    if (abs(denom) > 1e-6) {
        float t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
        if (t >= 0) {
            intersection = rayOrigin + rayDir * t;
            return true;
        }
    }
    return false;
}

// CameraPath implementation
CameraKeyframe CameraPath::interpolate(float time) const {
    if (keyframes.empty()) {
        return CameraKeyframe(glm::vec3(0), glm::vec3(0, 0, -1), 0);
    }

    if (keyframes.size() == 1) {
        return keyframes[0];
    }

    // Handle looping
    if (looping && time > totalDuration) {
        time = fmod(time, totalDuration);
    }

    // Find the two keyframes to interpolate between
    size_t i = 0;
    for (i = 0; i < keyframes.size() - 1; ++i) {
        if (time >= keyframes[i].timestamp && time <= keyframes[i + 1].timestamp) {
            break;
        }
    }

    if (i >= keyframes.size() - 1) {
        return keyframes.back();
    }

    const CameraKeyframe& k1 = keyframes[i];
    const CameraKeyframe& k2 = keyframes[i + 1];

    float t = (time - k1.timestamp) / (k2.timestamp - k1.timestamp);
    t = glm::clamp(t, 0.0f, 1.0f);

    // Smooth interpolation using smoothstep
    t = t * t * (3.0f - 2.0f * t);

    CameraKeyframe result(
        glm::mix(k1.position, k2.position, t),
        glm::mix(k1.target, k2.target, t),
        time,
        glm::mix(k1.fov, k2.fov, t)
    );

    return result;
}