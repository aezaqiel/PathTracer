#pragma once

#include "Events.hpp"

struct GLFWwindow;

class Window
{
    friend class Application;
    using EventCallbackFn = std::function<void(const Event&)>;
public:
    Window(u32 width, u32 height, const std::string& title);
    ~Window();

    inline u32 GetWidth() const { return m_Data.width; }
    inline u32 GetHeight() const { return m_Data.height; }
    inline GLFWwindow* GetNative() const { return m_Window; }

    std::string GetTitle() const;

    inline void BindEventCallback(const EventCallbackFn& callback)
    {
        m_Data.callback = callback;
    }

    static std::vector<const char*> GetRequiredVulkanExtensions();

protected:
    static void PollEvents();

private:
    struct WindowData
    {
        u32 width { 0 };
        u32 height { 0 };
        EventCallbackFn callback { nullptr };
    };

private:
    inline static std::atomic<usize> s_Instance { 0 };

    GLFWwindow* m_Window { nullptr };
    WindowData m_Data;
};
