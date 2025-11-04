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

        m_Camera = std::make_unique<Camera>(
            45.0f,
            static_cast<f32>(m_Window->GetWidth()) / static_cast<f32>(m_Window->GetHeight()),
            0.1f, 100.0f
        );

        m_Model = std::make_unique<Model>("Sponza/NewSponza_Main_glTF_003.gltf");

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

            m_Camera->OnUpdate(*m_Timer);

            if (!m_Minimized) {
                Renderer::RenderPacket packet;

                packet.camera = m_Camera.get();

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
            m_Camera->OnEvent(dispatcher);
        }
    }

}
