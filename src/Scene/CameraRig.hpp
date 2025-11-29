#pragma once

#include "Camera.hpp"
#include "Core/Input.hpp"

namespace Scene {

    struct DampedValue
    {
        glm::vec3 current { 0.0f };
        glm::vec3 target { 0.0f };
        glm::vec3 velocity { 0.0f };
        f32 smoothTime { 0.1f };

        inline void Update(f32 dt)
        {
            glm::vec3 diff = target - current;
            current += diff * glm::min(1.0f, dt / smoothTime);
        }
    };

    class CameraRig
    {
    public:
        virtual ~CameraRig() = default;
        virtual void Update(CameraState& state, f32 dt) = 0;
    };

}
