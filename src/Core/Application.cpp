#include "Application.hpp"

#include "Input.hpp"
#include "Scene/Rigs/FreeFlyRig.hpp"

#define BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

Application::Application()
{
    m_Window = std::make_shared<Window>(1280, 720, "PathTracer");
    m_Window->BindEventCallback(BIND_EVENT_FN(Application::DispatchEvents));

    m_Renderer = std::make_unique<Renderer>(m_Window, Renderer::Settings {
        .width = m_Window->GetWidth(),
        .height = m_Window->GetHeight(),
        .samples = 32
    });

    m_Camera = std::make_unique<Scene::CameraSystem>(m_Window->GetWidth(), m_Window->GetHeight());
    m_Camera->AddRig<Scene::FreeFlyRig>(Scene::FreeFlyRig::Settings {
        .moveSpeed = 5.0f,
        .moveBoost = 4.0f,
        .rotationSpeed = 0.1f,
        .damping = 0.2f
    });
}

void Application::Run()
{
    auto last = std::chrono::steady_clock::now();

    while (m_Running) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<f32> duration = now - last;
        f32 dt = duration.count();
        last = now;

        Window::PollEvents();

        if (Input::IsKeyDown(KeyCode::Escape)) {
            m_Running = false;
        }

        m_Camera->Update(dt);

        if (!m_Minimized) {
            auto cam = m_Camera->GetShaderData();
            m_Renderer->Draw(std::move(cam));
        }

        Input::Update();
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
    m_Camera->OnEvent(event);
    m_Renderer->OnEvent(event);
}
