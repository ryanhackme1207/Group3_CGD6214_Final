#include "Camera.h"
#include <cmath>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
    MovementSpeed(SPEED),
    MouseSensitivity(SENSITIVITY),
    Zoom(ZOOM),
    Mode(FLY_MODE),
    CarPosition(glm::vec3(0.0f, -0.8f, 0.0f)),
    CarYaw(0.0f),
    CarCameraYaw(0.0f),
    CarCameraPitch(0.0f),
    LastMouseMoveTime(0.0f),
    MouseControlActive(false) {
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() {
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    float velocity = MovementSpeed * deltaTime;

    if (Mode == FLY_MODE) {
        // Normal flying camera
        if (direction == FORWARD)
            Position += Front * velocity;
        if (direction == BACKWARD)
            Position -= Front * velocity;
        if (direction == LEFT)
            Position -= Right * velocity;
        if (direction == RIGHT)
            Position += Right * velocity;
    }
    else if (Mode == WALK_MODE) {
        // First person walking - only move on XZ plane
        glm::vec3 frontXZ = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
        glm::vec3 rightXZ = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

        if (direction == FORWARD)
            Position += frontXZ * velocity;
        if (direction == BACKWARD)
            Position -= frontXZ * velocity;
        if (direction == LEFT)
            Position -= rightXZ * velocity;
        if (direction == RIGHT)
            Position += rightXZ * velocity;

        // Keep at ground level
        Position.y = -0.5f; // Eye height above ground level
    }
    else if (Mode == CAR_MODE) {
        // GTA V style car driving controls
        static float carVelocity = 0.0f;
        static bool isReversing = false;

        const float maxSpeed = 15.0f;
        const float acceleration = 25.0f;
        const float braking = 35.0f;
        const float friction = 10.0f;
        const float turnSpeed = 120.0f;

        if (direction == FORWARD) {
            // W = Gas pedal (accelerate forward)
            if (isReversing) {
                // Braking from reverse
                carVelocity += braking * deltaTime;
                if (carVelocity >= 0.0f) {
                    carVelocity = 0.0f;
                    isReversing = false;
                }
            }
            else {
                // Accelerating forward
                carVelocity += acceleration * deltaTime;
                if (carVelocity > maxSpeed) carVelocity = maxSpeed;
            }
        }
        else if (direction == BACKWARD) {
            // S = Brake or Reverse
            if (carVelocity > 0.0f) {
                // Braking
                carVelocity -= braking * deltaTime;
                if (carVelocity < 0.0f) carVelocity = 0.0f;
            }
            else {
                // Reversing
                carVelocity -= acceleration * deltaTime * 0.6f; // Slower reverse
                if (carVelocity < -maxSpeed * 0.5f) carVelocity = -maxSpeed * 0.5f;
                isReversing = true;
            }
        }
        else {
            // Natural friction when no gas/brake
            if (carVelocity > 0.0f) {
                carVelocity -= friction * deltaTime;
                if (carVelocity < 0.0f) carVelocity = 0.0f;
            }
            else if (carVelocity < 0.0f) {
                carVelocity += friction * deltaTime;
                if (carVelocity > 0.0f) carVelocity = 0.0f;
            }
        }

        // Steering (only works when moving)
        if (fabsf(carVelocity) > 0.1f) {
            float steerMultiplier = fabsf(carVelocity) / maxSpeed; // More responsive at higher speeds
            if (direction == LEFT) {
                CarYaw -= turnSpeed * steerMultiplier * deltaTime;
            }
            if (direction == RIGHT) {
                CarYaw += turnSpeed * steerMultiplier * deltaTime;
            }
        }

        // Move car based on velocity and direction
        if (fabsf(carVelocity) > 0.01f) {
            CarPosition.x += sinf(glm::radians(CarYaw)) * carVelocity * deltaTime;
            CarPosition.z += cosf(glm::radians(CarYaw)) * carVelocity * deltaTime;
        }

        // Keep car on ground
        CarPosition.y = -0.3f;

        // Update mouse timeout timer
        LastMouseMoveTime += deltaTime;

        // Auto-center camera if mouse hasn't moved for 2 seconds
        if (LastMouseMoveTime > 2.0f && MouseControlActive) {
            MouseControlActive = false;
            // Smoothly reset camera to forward direction
            CarCameraYaw *= 0.95f; // Smooth transition back to center
            CarCameraPitch *= 0.95f;
            if (fabsf(CarCameraYaw) < 0.5f) CarCameraYaw = 0.0f;
            if (fabsf(CarCameraPitch) < 0.5f) CarCameraPitch = 0.0f;
        }

        // Calculate camera position behind car
        glm::vec3 carForward = glm::vec3(sinf(glm::radians(CarYaw)), 0.0f, cosf(glm::radians(CarYaw)));
        Position = CarPosition - carForward * 8.0f + glm::vec3(0.0f, 3.0f, 0.0f);

        // Set camera direction based on car + mouse control
        Yaw = CarYaw + CarCameraYaw;
        Pitch = CarCameraPitch;

        updateCameraVectors();
    }
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    if (Mode == CAR_MODE) {
        // Car camera control - use separate camera yaw/pitch
        CarCameraYaw += xoffset;
        CarCameraPitch += yoffset;

        // Constrain pitch for car camera
        if (CarCameraPitch > 60.0f)
            CarCameraPitch = 60.0f;
        if (CarCameraPitch < -30.0f)
            CarCameraPitch = -30.0f;

        // Update mouse activity tracking
        MouseControlActive = true;
        LastMouseMoveTime = 0.0f; // Reset timer

        // Update camera angles for car mode
        Yaw = CarYaw + CarCameraYaw;
        Pitch = CarCameraPitch;
        updateCameraVectors();
    }
    else {
        // Normal camera control for fly/walk modes
        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }
        updateCameraVectors();
    }
}

void Camera::ProcessMouseScroll(float yoffset) {
    Zoom -= (float)yoffset;
    if (Zoom < 1.0f)
        Zoom = 1.0f;
    if (Zoom > 45.0f)
        Zoom = 45.0f;
}

void Camera::SetMode(Camera_Mode mode) {
    Mode = mode;
    if (mode == WALK_MODE) {
        Position.y = 0.8f; // Set eye height
    }
    else if (mode == CAR_MODE) {
        CarPosition = glm::vec3(Position.x, -0.3f, Position.z);
        CarYaw = Yaw + 90.0f;

        // Initialize car camera control
        CarCameraYaw = 0.0f;
        CarCameraPitch = 0.0f;
        LastMouseMoveTime = 0.0f;
        MouseControlActive = false;
    }
}

void Camera::UpdateWalkMode() {
    // Constrain pitch in walk mode for more natural movement
    if (Pitch > 60.0f) Pitch = 60.0f;
    if (Pitch < -60.0f) Pitch = -60.0f;
}

void Camera::UpdateCarMode() {
    // Update camera position based on car position
    Position = CarPosition + glm::vec3(0.0f, 1.2f, 0.0f);
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cosf(glm::radians(Yaw)) * cosf(glm::radians(Pitch));
    front.y = sinf(glm::radians(Pitch));
    front.z = sinf(glm::radians(Yaw)) * cosf(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}
