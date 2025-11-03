#include "Core/Logger.hpp"
#include "Core/Application.hpp"

int main()
{
    PathTracer::Logger::Init();
    {
        PathTracer::Application app;
        app.Run();
    }
    PathTracer::Logger::Shutdown();
}
