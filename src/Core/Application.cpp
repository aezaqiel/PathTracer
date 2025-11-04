#include "Application.hpp"

#include <stb_image_write.h>

#include "Input.hpp"

namespace PathTracer {

    Application::Application()
    {
        m_Timer = std::make_unique<Timer>();

        m_EventQueue = std::make_unique<EventQueue>();

        m_Window = std::make_shared<Window>(Window::Config(1280, 720, "PathTracer"));
        m_Window->BindEventQueue(m_EventQueue.get());

        m_Renderer = std::make_unique<Renderer>(m_Window);
    }

    void Application::Run()
    {
        while (m_Running) {
            m_Timer->Tick();

            Window::PollEvents();
            Input::Update();

            ProcessEvents();

            if (Input::IsKeyPressed(KeyCode::Escape)) {
                m_Running = false;
            }

            if (!m_Minimized) {
                Renderer::RenderPacket packet;
                m_Renderer->Submit(packet);
            }
        }
    }

    void Application::ProcessEvents()
    {
        for (auto& event : m_EventQueue->Poll()) {
            EventDispatcher dispatcher(event);

            dispatcher.Dispatch<WindowClosedEvent>([&](const WindowClosedEvent&) {
                m_Running = false;
                return true;
            });

            dispatcher.Dispatch<WindowMinimizeEvent>([&](const WindowMinimizeEvent& e) {
                m_Minimized = e.minimized;
                return false;
            });

            dispatcher.Dispatch<WindowResizedEvent>([&](const WindowResizedEvent& e) {
                m_Renderer->RequestResize(e.width, e.height);
                return false;
            });

            Input::OnEvent(dispatcher);
        }
    }

}
