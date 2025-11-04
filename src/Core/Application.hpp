#pragma once

#include "Timer.hpp"
#include "Events.hpp"
#include "Window.hpp"

#include "Scene/AssetManager.hpp"
#include "Scene/Camera.hpp"

#include "Renderer/Renderer.hpp"

namespace PathTracer {

    class Application
    {
    public:
        Application();
        ~Application() = default;

        void Run();

    private:
        void ProcessEvents();
    
    private:
        bool m_Running { true };
        bool m_Minimized { false };

        std::unique_ptr<Timer> m_Timer;

        std::unique_ptr<EventQueue> m_EventQueue;
        std::shared_ptr<Window> m_Window;

        std::unique_ptr<AssetManager> m_AssetManager;
        std::unique_ptr<Renderer> m_Renderer;

        std::unique_ptr<Camera> m_Camera;
    };

}
