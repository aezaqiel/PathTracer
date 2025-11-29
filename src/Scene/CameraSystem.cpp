#include "CameraSystem.hpp"

namespace Scene {

    CameraSystem::CameraSystem(u32 width, u32 height)
        : m_AspectRatio(static_cast<f32>(width) / static_cast<f32>(height))
    {
    }

    void CameraSystem::OnEvent(const Event& event)
    {
        EventDispatcher dispatcher(event);

        dispatcher.Dispatch<WindowResizedEvent>([&](const WindowResizedEvent& e) {
            m_AspectRatio = static_cast<f32>(e.width) / static_cast<f32>(e.height);
        });
    }

    void CameraSystem::Update(f32 dt)
    {
        CameraState state;

        if (!m_Rigs.empty()) {
            m_Rigs.back()->Update(state, dt);
        }

        m_State = state;
        UpdateMatrices();
    }

    void CameraSystem::UpdateMatrices()
    {
        glm::mat4 view = glm::translate(glm::mat4_cast(glm::conjugate(m_State.rotation)), -m_State.position);
        glm::mat4 proj = glm::perspective(glm::radians(m_State.vFOV), m_AspectRatio, 0.001f, 1000.0f);

        proj[1][1] *= -1;

        m_Data.inverseView = glm::inverse(view);
        m_Data.inverseProj = glm::inverse(proj);
        m_Data.position = glm::vec4(m_State.position, 1.0f);
        m_Data.params = glm::vec4(
            m_State.vFOV,
            m_AspectRatio,
            0.001f,
            1000.0f
        );
    }

}
