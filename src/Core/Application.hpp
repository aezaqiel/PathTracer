#pragma once

#include "Events.hpp"
#include "Window.hpp"
#include "Renderer/Renderer.hpp"
#include "Scene/CameraSystem.hpp"

class Application
{
public:
    Application();
    ~Application() = default;

    void Run();

private:
    void DispatchEvents(const Event& event);

private:
    bool m_Running { true };
    bool m_Minimized { false };

    std::shared_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;

    std::unique_ptr<Scene::CameraSystem> m_Camera;
};
