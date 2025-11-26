#include "Window.hpp"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

Window::Window(u32 width, u32 height, const std::string& title)
{
    if (s_Instance.fetch_add(1, std::memory_order_relaxed) == 0) {
        glfwSetErrorCallback([](i32 code, const char* desc) {
            LOG_ERROR("GLFW Error {}: {}", code, desc);
        });

        glfwInit();
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_Window = glfwCreateWindow(
        static_cast<i32>(width),
        static_cast<i32>(height),
        title.c_str(),
        nullptr, nullptr
    );

    {
        i32 w, h;
        glfwGetFramebufferSize(m_Window, &w, &h);
        m_Data.width = static_cast<u32>(w);
        m_Data.height = static_cast<u32>(h);
    }

    glfwSetWindowUserPointer(m_Window, &m_Data);

    glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (data.callback) {
            data.callback(WindowClosedEvent());
        }
    });

    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, i32 w, i32 h) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        data.width = static_cast<u32>(w);
        data.height = static_cast<u32>(h);

        if (data.callback) {
            data.callback(WindowResizedEvent(data.width, data.height));
        }
    });

    glfwSetWindowIconifyCallback(m_Window, [](GLFWwindow* window, i32 iconified) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        if (data.callback) {
            data.callback(WindowMinimizeEvent(static_cast<bool>(iconified)));
        }
    });

    glfwSetKeyCallback(m_Window, [](GLFWwindow* window, i32 key, i32, i32 action, i32) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        if (!data.callback) return;

        switch (action) {
            case GLFW_PRESS: {
                data.callback(KeyPressedEvent(static_cast<KeyCode>(key), false));
            } break;
            case GLFW_RELEASE: {
                data.callback(KeyReleasedEvent(static_cast<KeyCode>(key)));
            } break;
            case GLFW_REPEAT: {
                data.callback(KeyPressedEvent(static_cast<KeyCode>(key), true));
            } break;
            default:
                LOG_WARN("Unknown key action {}", action);
        }
    });

    glfwSetCharCallback(m_Window, [](GLFWwindow* window, u32 code) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        if (data.callback) {
            data.callback(KeyTypedEvent(code));
        }
    });

    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, i32 button, i32 action, i32) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        if (!data.callback) return;

        switch (action) {
            case GLFW_PRESS: {
                data.callback(MouseButtonPressedEvent(static_cast<MouseButton>(button)));
            } break;
            case GLFW_RELEASE: {
                data.callback(MouseButtonReleasedEvent(static_cast<MouseButton>(button)));
            } break;
            default:
                LOG_WARN("Unknown mouse button action {}", action);
        }
    });

    glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, f64 x, f64 y) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        if (data.callback) {
            data.callback(MouseMovedEvent(static_cast<f32>(x), static_cast<f32>(y)));
        }
    });

    glfwSetScrollCallback(m_Window, [](GLFWwindow* window, f64 x, f64 y) {
        WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        if (data.callback) {
            data.callback(MouseScrolledEvent(static_cast<f32>(x), static_cast<f32>(y)));
        }
    });

    LOG_INFO("Created window \"{}\" ({}, {})", GetTitle(), m_Data.width, m_Data.height);
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
    m_Window = nullptr;

    if (s_Instance.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        glfwTerminate();
    }
}

std::string Window::GetTitle() const
{
    return std::string(glfwGetWindowTitle(m_Window));
}

std::vector<const char*> Window::GetRequiredVulkanExtensions()
{
    u32 count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);

    return std::vector<const char*>(extensions, extensions + count);
}

void Window::PollEvents()
{
    glfwPollEvents();
}
