#include "Camera.h"
#include <algorithm>
#include <iostream>

// Constructor with vectors
Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
{
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;

    // Initialize smooth movement
    SmoothMovement = false;
    Smoothness = 5.0f;
    TargetPosition = position;
    Velocity = glm::vec3(0.0f);

    // Initialize boundaries (large default area)
    BoundaryMinX = -100.0f;
    BoundaryMaxX = 100.0f;
    BoundaryMinZ = -100.0f;
    BoundaryMaxZ = 100.0f;
    BoundaryMinY = 1.0f;
    BoundaryMaxY = 50.0f;
    CollisionRadius = 2.0f;

    // Initialize camera mode
    Mode = FREE_FLY;
    OrbitTarget = glm::vec3(0.0f);
    OrbitDistance = 15.0f;
    OrbitSpeed = 1.0f;

    // Initialize transition state
    isTransitioning = false;
    transitionTime = 0.0f;
    transitionDuration = 1.0f;

    updateCameraVectors();
}

// Constructor with scalar values
Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch)
    : Camera(glm::vec3(posX, posY, posZ), glm::vec3(upX, upY, upZ), yaw, pitch)
{
}

// Returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 Camera::GetViewMatrix()
{
    if (Mode == ORBITAL) {
        glm::vec3 eye = OrbitTarget + glm::vec3(
            cos(glm::radians(Yaw)) * cos(glm::radians(Pitch)) * OrbitDistance,
            sin(glm::radians(Pitch)) * OrbitDistance,
            sin(glm::radians(Yaw)) * cos(glm::radians(Pitch)) * OrbitDistance
        );
        return glm::lookAt(eye, OrbitTarget, WorldUp);
    }

    return glm::lookAt(Position, Position + Front, Up);
}

// Returns the projection matrix
glm::mat4 Camera::GetProjectionMatrix(float aspectRatio, float nearPlane, float farPlane)
{
    return glm::perspective(glm::radians(Zoom), aspectRatio, nearPlane, farPlane);
}

// Processes input received from any keyboard-like input system
void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime)
{
    float velocity = MovementSpeed * deltaTime;
    glm::vec3 newPosition = Position;

    if (Mode == ORBITAL) {
        // In orbital mode, WASD controls orbit angle and distance
        if (direction == FORWARD)
            OrbitDistance = std::max(2.0f, OrbitDistance - velocity * 2.0f);
        if (direction == BACKWARD)
            OrbitDistance = std::min(50.0f, OrbitDistance + velocity * 2.0f);
        if (direction == LEFT)
            Yaw -= velocity * 30.0f;
        if (direction == RIGHT)
            Yaw += velocity * 30.0f;
        if (direction == UP)
            Pitch = std::min(89.0f, Pitch + velocity * 30.0f);
        if (direction == DOWN)
            Pitch = std::max(-89.0f, Pitch - velocity * 30.0f);
        return;
    }

    // Standard movement for other modes
    if (direction == FORWARD)
        newPosition += Front * velocity;
    if (direction == BACKWARD)
        newPosition -= Front * velocity;
    if (direction == LEFT)
        newPosition -= Right * velocity;
    if (direction == RIGHT)
        newPosition += Right * velocity;
    if (direction == UP)
        newPosition += WorldUp * velocity;
    if (direction == DOWN)
        newPosition -= WorldUp * velocity;

    // Apply movement immediately (disable smooth movement for debugging)
    Position = newPosition;
    ApplyBoundaries();

    // Debug output (remove later)
    std::cout << "Camera Position: (" << Position.x << ", " << Position.y << ", " << Position.z << ")" << std::endl;
}

// Processes input received from a mouse input system
void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    // Update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

// Processes input received from a mouse scroll-wheel event
void Camera::ProcessMouseScroll(float yoffset)
{
    Zoom -= yoffset;
    if (Zoom < MIN_ZOOM)
        Zoom = MIN_ZOOM;
    if (Zoom > MAX_ZOOM)
        Zoom = MAX_ZOOM;
}

// Set camera boundaries
void Camera::SetBoundaries(float minX, float maxX, float minZ, float maxZ, float minY, float maxY)
{
    BoundaryMinX = minX;
    BoundaryMaxX = maxX;
    BoundaryMinZ = minZ;
    BoundaryMaxZ = maxZ;
    BoundaryMinY = minY;
    BoundaryMaxY = maxY;
}

// Set collision radius
void Camera::SetCollisionRadius(float radius)
{
    CollisionRadius = radius;
}

// Check collision with buildings
bool Camera::CheckCollisionWithBuilding(glm::vec3 newPosition, const std::vector<glm::vec3>& buildingPositions,
    const std::vector<glm::vec3>& buildingSizes)
{
    for (size_t i = 0; i < buildingPositions.size(); ++i) {
        glm::vec3 buildingPos = buildingPositions[i];
        glm::vec3 buildingSize = buildingSizes[i];

        // Check if camera is inside building bounds (with collision radius)
        if (newPosition.x + CollisionRadius > buildingPos.x - buildingSize.x / 2.0f &&
            newPosition.x - CollisionRadius < buildingPos.x + buildingSize.x / 2.0f &&
            newPosition.z + CollisionRadius > buildingPos.z - buildingSize.z / 2.0f &&
            newPosition.z - CollisionRadius < buildingPos.z + buildingSize.z / 2.0f &&
            newPosition.y < buildingPos.y + buildingSize.y) {
            return true; // Collision detected
        }
    }
    return false; // No collision
}

// Apply boundary constraints
void Camera::ApplyBoundaries()
{
    Position.x = std::max(BoundaryMinX, std::min(BoundaryMaxX, Position.x));
    Position.y = std::max(BoundaryMinY, std::min(BoundaryMaxY, Position.y));
    Position.z = std::max(BoundaryMinZ, std::min(BoundaryMaxZ, Position.z));
}

// Enable or disable smooth movement
void Camera::EnableSmoothMovement(bool enable, float smoothness)
{
    SmoothMovement = enable;
    Smoothness = smoothness;
    if (enable) {
        TargetPosition = Position;
        Velocity = glm::vec3(0.0f);
    }
}

// Update smooth movement
void Camera::UpdateSmoothMovement(float deltaTime)
{
    if (!SmoothMovement) return;

    // Calculate velocity towards target
    glm::vec3 direction = TargetPosition - Position;
    float distance = glm::length(direction);

    if (distance > 0.01f) { // Only move if significant distance
        // Apply spring-like movement
        Velocity += direction * Smoothness * deltaTime;
        Velocity *= 0.8f; // Damping

        Position += Velocity * deltaTime;
        ApplyBoundaries();
    }
}

// Set target position for smooth movement
void Camera::SetTargetPosition(glm::vec3 target)
{
    TargetPosition = target;
}

// Set camera mode
void Camera::SetCameraMode(CameraMode mode)
{
    Mode = mode;
    if (mode == ORBITAL && OrbitTarget == glm::vec3(0.0f)) {
        // Set default orbit target to scene center
        OrbitTarget = glm::vec3(0.0f, 5.0f, 0.0f);
    }
}

// Update orbital camera
void Camera::UpdateOrbitalCamera(float deltaTime)
{
    if (Mode != ORBITAL) return;

    // Auto-rotation can be added here if desired
    // Yaw += OrbitSpeed * deltaTime * 10.0f; // Uncomment for auto-rotation
}

// Set orbit target
void Camera::SetOrbitTarget(glm::vec3 target, float distance)
{
    OrbitTarget = target;
    OrbitDistance = distance;
}

// Reset camera to default values
void Camera::ResetToDefault()
{
    Position = glm::vec3(0.0f, 5.0f, 10.0f);
    Yaw = YAW;
    Pitch = PITCH;
    Zoom = ZOOM;
    MovementSpeed = SPEED;
    MouseSensitivity = SENSITIVITY;
    Mode = FREE_FLY;
    OrbitDistance = 15.0f;

    updateCameraVectors();
}

// Smooth transition between positions
void Camera::SmoothTransitionTo(glm::vec3 newPosition, glm::vec3 newTarget, float duration)
{
    isTransitioning = true;
    transitionTime = 0.0f;
    transitionDuration = duration;
    startPosition = Position;
    endPosition = newPosition;

    // Calculate target front direction
    glm::vec3 targetDirection = glm::normalize(newTarget - newPosition);
    startTarget = Position + Front;
    endTarget = newPosition + targetDirection;
}

// Update transition
void Camera::UpdateTransition(float deltaTime)
{
    if (!isTransitioning) return;

    transitionTime += deltaTime;
    float t = transitionTime / transitionDuration;

    if (t >= 1.0f) {
        t = 1.0f;
        isTransitioning = false;
    }

    // Apply smooth interpolation
    float smoothT = smoothstep(t);
    Position = lerp(startPosition, endPosition, smoothT);

    // Update front vector based on interpolated target
    glm::vec3 currentTarget = lerp(startTarget, endTarget, smoothT);
    Front = glm::normalize(currentTarget - Position);

    // Recalculate camera vectors
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}

// Private helper functions
void Camera::updateCameraVectors()
{
    // Calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    // Also re-calculate the Right and Up vector
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}

// Linear interpolation
float Camera::lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

// Vector linear interpolation
glm::vec3 Camera::lerp(const glm::vec3& a, const glm::vec3& b, float t)
{
    return a + t * (b - a);
}

// Smooth step function for smoother transitions
float Camera::smoothstep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}