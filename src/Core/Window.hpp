#pragma once

#include "Types.hpp"
#include "Events.hpp"

struct GLFWwindow;

namespace PathTracer {

    class Window
    {
        friend class Application;
    public:
        struct Config
        {
            u32 width;
            u32 height;
            std::string title;

            constexpr Config(u32 width = 1280, u32 height = 720, const std::string_view& title = "Window") noexcept
                : width(width), height(height), title(title) {}
        };

    public:
        Window(const Config& config = Config());
        ~Window();

        inline u32 GetWidth() const { return m_Data.width.load(std::memory_order_relaxed); }
        inline u32 GetHeight() const { return m_Data.height.load(std::memory_order_relaxed); }
        inline GLFWwindow* GetNative() const { return m_Window; }

        inline void BindEventQueue(EventQueue* queue)
        {
            m_Data.queue = queue;
        }

    protected:
        static void PollEvents();

    private:
        struct WindowData
        {
            std::atomic<u32> width { 0 };
            std::atomic<u32> height { 0 };
            EventQueue* queue { nullptr };
        };

    private:
        inline static std::atomic<usize> s_InstanceCount { 0 };

        GLFWwindow* m_Window { nullptr };
        WindowData m_Data;
    };

}
