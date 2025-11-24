#include "Logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

void Logger::Init()
{
    if (s_Initialized) return;

    std::vector<spdlog::sink_ptr> sinks {
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
    };

    for (auto& sink : sinks) {
        sink->set_pattern("[%H:%M:%S %z] [%^%l%$] [thread %t] %v");
    }

    auto logger = std::make_shared<spdlog::logger>("RayTracing", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::err);

    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);

    s_Initialized = true;
}

void Logger::Shutdown()
{
    if (s_Initialized) {
        spdlog::shutdown();
        s_Initialized = false;
    }
}
