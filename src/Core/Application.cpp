#include "Application.hpp"

#include "Window.hpp"
#include "Input.hpp"

#include "Renderer/Renderer.hpp"

#define BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

Application::Application()
{
    m_Window = std::make_shared<Window>(1280, 720, "PathTracer");
    m_Window->BindEventCallback(BIND_EVENT_FN(Application::DispatchEvents));

    m_Renderer = std::make_unique<Renderer>(m_Window);
}

Application::~Application()
{
}

void Application::Run()
{
    while (m_Running) {
        Window::PollEvents();
        Input::Update();

        if (Input::IsKeyDown(KeyCode::Escape)) {
            m_Running = false;
        }

        if (!m_Minimized) {
            m_Renderer->Draw();
        }
    }
}

void Application::DispatchEvents(const Event& event)
{
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<WindowClosedEvent>([&](const WindowClosedEvent&) {
        m_Running = false;
        return true;
    });

    dispatcher.Dispatch<WindowMinimizeEvent>([&](const WindowMinimizeEvent& e) {
        m_Minimized = e.minimized;
    });

    Input::OnEvent(event);
}
