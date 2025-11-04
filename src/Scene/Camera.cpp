#include "Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "Core/Input.hpp"

namespace PathTracer {

    Camera::Camera(f32 fov, f32 aspect, f32 near, f32 far)
        : m_FOV(fov), m_AspectRatio(aspect), m_NearClip(near), m_FarClip(far)
    {
        UpdateCameraVectors();
        RecalculateProjectionMatrix();
        RecalculateViewMatrix();

        auto [x, y] = Input::GetMousePosition();
        m_LastMousePos = { x, y };
    }

    void Camera::OnUpdate(const Timer& time)
    {
        f32 velocity = m_MoveSpeed * time.GetDeltaTime();

        if (Input::IsKeyDown(KeyCode::W)) {
            m_Position += m_Forward * velocity;
        } else if (Input::IsKeyDown(KeyCode::S)) {
            m_Position -= m_Forward * velocity;
        }

        if (Input::IsKeyDown(KeyCode::A)) {
            m_Position -= m_Right * velocity;
        } else if (Input::IsKeyDown(KeyCode::D)) {
            m_Position += m_Right * velocity;
        }

        if (Input::IsKeyDown(KeyCode::Space)) {
            m_Position -= m_WorldUp * velocity;
        } else if (Input::IsKeyDown(KeyCode::LeftShift)) {
            m_Position += m_WorldUp * velocity;
        }

        if (Input::IsMouseButtonDown(MouseButton::Left)) {
            auto [x, y] = Input::GetMousePosition();
            glm::vec2 mousePos = { x, y };

            if (m_FirstMouse) {
                m_LastMousePos = mousePos;
                m_FirstMouse = false;
            }

            glm::vec2 delta = (mousePos - m_LastMousePos) * m_MouseSensitivity;
            m_LastMousePos = mousePos;

            m_Yaw += delta.x;
            m_Pitch += delta.y;

            if (m_Pitch > 89.0f) m_Pitch = 89.0f;
            if (m_Pitch < -89.0f) m_Pitch = -89.0f;

            UpdateCameraVectors();
            RecalculateViewMatrix();
        } else {
            m_FirstMouse = true;
        }
    }

    void Camera::OnEvent(EventDispatcher& dispatcher)
    {
        dispatcher.Dispatch<WindowResizedEvent>([&](const WindowResizedEvent& e) {
            SetViewportSize(static_cast<f32>(e.width), static_cast<f32>(e.height));
            return false;
        });
    }

    void Camera::SetViewportSize(f32 width, f32 height)
    {
        if (width <= 0 || height <= 0) {
            return;
        }

        m_AspectRatio = width / height;
        RecalculateProjectionMatrix();
    }

    void Camera::RecalculateViewMatrix()
    {
        m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
    }

    void Camera::RecalculateProjectionMatrix()
    {
        m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
    }

    void Camera::UpdateCameraVectors()
    {
        glm::vec3 front(
            glm::cos(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch)),
            glm::sin(glm::radians(m_Pitch)),
            glm::sin(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch))
        );

        m_Forward = glm::normalize(front);
        m_Right = glm::normalize(glm::cross(m_Forward, m_WorldUp));
        m_Up = glm::normalize(glm::cross(m_Right, m_Forward));
    }

}
