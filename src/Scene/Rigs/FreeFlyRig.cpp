#include "FreeFlyRig.hpp"

namespace Scene {

    FreeFlyRig::FreeFlyRig(const Settings& settings)
        : m_Settings(settings)
    {
    }

    void FreeFlyRig::Update(CameraState& state, f32 dt)
    {
        glm::vec3 inputDir(0.0f);

        f32 speed = m_Settings.moveSpeed;
        if (Input::IsKeyDown(KeyCode::LeftShift)) speed *= m_Settings.moveBoost;

        if (Input::IsKeyDown(KeyCode::W)) inputDir.z -= 1.0f;
        if (Input::IsKeyDown(KeyCode::S)) inputDir.z += 1.0f;
        if (Input::IsKeyDown(KeyCode::A)) inputDir.x -= 1.0f;
        if (Input::IsKeyDown(KeyCode::D)) inputDir.x += 1.0f;
        if (Input::IsKeyDown(KeyCode::E)) inputDir.y += 1.0f;
        if (Input::IsKeyDown(KeyCode::Q)) inputDir.y -= 1.0f;

        if (glm::length(inputDir) > 0.0f) inputDir = glm::normalize(inputDir);

        if (Input::IsMouseButtonPressed(MouseButton::Right)) {
            m_LastMousePos = { Input::GetMouseX(), Input::GetMouseY() };
        } else if (Input::IsMouseButtonHeld(MouseButton::Right)) {
            glm::vec2 currentPos = { Input::GetMouseX(), Input::GetMouseY() };
            glm::vec2 delta = currentPos - m_LastMousePos;
            m_LastMousePos = currentPos;

            m_Yaw -= delta.x * m_Settings.rotationSpeed;
            m_Pitch -= delta.y * m_Settings.rotationSpeed;

            m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);
        }

        glm::vec3 targetVel = inputDir * speed;
        m_Velocity = glm::mix(m_Velocity, targetVel, dt / m_Settings.damping);

        glm::quat qPitch = glm::angleAxis(glm::radians(m_Pitch), glm::vec3(1, 0, 0));
        glm::quat qYaw = glm::angleAxis(glm::radians(m_Yaw), glm::vec3(0, 1, 0));
        glm::quat orientation = qYaw * qPitch;

        glm::vec3 forward = orientation * glm::vec3(0, 0, 1);
        glm::vec3 right = orientation * glm::vec3(1, 0, 0);
        glm::vec3 up = glm::vec3(0, 1, 0);

        m_CurrentPos += (right * m_Velocity.x) * dt;
        m_CurrentPos += (up * m_Velocity.y) * dt;
        m_CurrentPos += (forward * m_Velocity.z) * dt;

        state.position = m_CurrentPos;
        state.rotation = orientation;
    }

}
