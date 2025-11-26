#pragma once

#include "Events.hpp"

class Window;
class Renderer;

class Application
{
public:
    Application();
    ~Application();

    void Run();

private:
    void DispatchEvents(const Event& event);

private:
    bool m_Running { true };
    bool m_Minimized { false };

    std::shared_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
};
