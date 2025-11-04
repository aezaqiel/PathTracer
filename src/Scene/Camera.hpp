#pragma once

#include <glm/glm.hpp>

#include "Core/Types.hpp"
#include "Core/Timer.hpp"
#include "Core/Events.hpp"

namespace PathTracer {

    class Camera
    {
    public:
        Camera(f32 fov, f32 aspect, f32 near, f32 far);
        ~Camera() = default;

        void OnUpdate(const Timer& time);
        void OnEvent(EventDispatcher& dispatcher);

        void SetViewportSize(f32 width, f32 height);

        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        const glm::mat4& GetProjMatrix() const { return m_ProjectionMatrix; }
        const glm::vec3& GetPosition() const { return m_Position; }

    private:
        void RecalculateViewMatrix();
        void RecalculateProjectionMatrix();
        void UpdateCameraVectors();

    private:
        glm::mat4 m_ViewMatrix { 1.0f };
        glm::mat4 m_ProjectionMatrix { 1.0f };

        glm::vec3 m_Position { 0.0f, 0.0f, -2.0f };
        glm::vec3 m_Forward { 0.0f, 0.0f, 1.0f };
        glm::vec3 m_Up { 0.0f, 1.0f, 0.0f };
        glm::vec3 m_Right { 1.0f, 0.0f, 0.0f };
        glm::vec3 m_WorldUp { 0.0f, 1.0f, 0.0f };

        f32 m_FOV;
        f32 m_AspectRatio;
        f32 m_NearClip;
        f32 m_FarClip;

        f32 m_Yaw { 90.0f };
        f32 m_Pitch { 0.0f };

        f32 m_MoveSpeed { 5.0f };
        f32 m_MouseSensitivity { 0.1f };

        glm::vec2 m_LastMousePos { 0.0f, 0.0f };
        bool m_FirstMouse { true };
    };

}
