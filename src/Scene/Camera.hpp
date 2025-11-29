#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Scene {

    struct CameraData
    {
        glm::mat4 inverseView;
        glm::mat4 inverseProj;
        glm::vec4 position;
        glm::vec4 params;
    };

    struct CameraState
    {
        glm::vec3 position { 0.0f };
        glm::quat rotation { 1.0f, 0.0, 0.0f, 0.0f };

        f32 vFOV { 45.0f };
        f32 focalDistance { 10.0f };
        f32 aperture { 16.0f };
    };

}
