#include "Core/Application.hpp"

int main()
{
    Logger::Init();

    Application* app = new Application();
    app->Run();
    delete app;

    Logger::Shutdown();
}
