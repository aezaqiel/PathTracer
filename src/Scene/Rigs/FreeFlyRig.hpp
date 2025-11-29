#pragma once

#include "Scene/CameraRig.hpp"

namespace Scene {

    class FreeFlyRig : public CameraRig
    {
    public:
        struct Settings
        {
            f32 moveSpeed { 5.0f };
            f32 moveBoost { 4.0f };
            f32 rotationSpeed { 0.2f };
            f32 damping { 0.2f };
        };

    public:
        FreeFlyRig(const Settings& settings);
        virtual ~FreeFlyRig() = default;

        virtual void Update(CameraState& state, f32 dt) override;

    private:
        Settings m_Settings;

        glm::vec3 m_Velocity { 0.0f };
        glm::vec2 m_RotationVelocity { 0.0f };

        glm::vec2 m_LastMousePos { 0.0f };

        glm::vec3 m_CurrentPos { 0.0f, 0.0f, 4.0f };
        f32 m_Pitch { 0.0f };
        f32 m_Yaw { 0.0f };
    };

}
