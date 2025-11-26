#pragma once

#include "Events.hpp"

class Window;

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

    std::unique_ptr<Window> m_Window;
};
