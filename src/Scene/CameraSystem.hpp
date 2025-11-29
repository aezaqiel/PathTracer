#pragma once

#include "CameraRig.hpp"
#include "Core/Events.hpp"

namespace Scene {

    class CameraSystem
    {
    public:
        CameraSystem(u32 width, u32 height);

        void OnEvent(const Event& event);
        void Update(f32 dt);

        CameraData GetShaderData() const { return m_Data; }

        template <typename T, typename... Args>
            requires std::is_constructible_v<T, Args...> && std::is_base_of_v<CameraRig, T>
        std::shared_ptr<T> AddRig(Args&&... args)
        {
            auto rig = std::make_shared<T>(std::forward<Args>(args)...);
            m_Rigs.push_back(rig);
            return rig;
        }

    private:
        void UpdateMatrices();

    private:
        std::vector<std::shared_ptr<CameraRig>> m_Rigs;
        CameraState m_State;
        CameraData m_Data;
        f32 m_AspectRatio { 1.0f };
    };

}
