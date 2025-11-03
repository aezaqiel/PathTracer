#include "Window.hpp"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Logger.hpp"

namespace PathTracer {

    Window::Window(const Config& config)
    {
        if (s_InstanceCount.fetch_add(1, std::memory_order_relaxed) == 0) {
            glfwSetErrorCallback([](i32 code, const char* desc) {
                LOG_ERROR("GLFW Error {}: {}", code, desc);
            });

            glfwInit();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_Window = glfwCreateWindow(
            static_cast<i32>(config.width), static_cast<i32>(config.height),
            config.title.c_str(),
            nullptr, nullptr
        );

        {
            i32 w, h;
            glfwGetFramebufferSize(m_Window, &w, &h);
            m_Data.width.store(static_cast<u32>(w), std::memory_order_relaxed);
            m_Data.height.store(static_cast<u32>(h), std::memory_order_relaxed);
        }

        glfwSetWindowUserPointer(m_Window, &m_Data);

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data.queue) {
                data.queue->Push(WindowClosedEvent());
            }
        });

        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, i32 width, i32 height) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.width.store(static_cast<u32>(width), std::memory_order_relaxed);
            data.height.store(static_cast<u32>(height), std::memory_order_relaxed);

            if (data.queue) {
                data.queue->Push(WindowResizedEvent(width, height));
            }
        });

        glfwSetWindowPosCallback(m_Window, [](GLFWwindow* window, i32 x, i32 y) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                data.queue->Push(WindowMovedEvent(x, y));
            }
        });

        glfwSetWindowIconifyCallback(m_Window, [](GLFWwindow* window, i32 iconified) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                data.queue->Push(WindowMinimizeEvent(iconified));
            }
        });

        glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* window, i32 focused) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                data.queue->Push(WindowFocusEvent(focused));
            }
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, i32 key, i32, i32 action, i32) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                switch (action) {
                    case GLFW_PRESS: {
                        data.queue->Push(KeyPressedEvent(static_cast<KeyCode>(key), false));
                    } break;
                    case GLFW_RELEASE: {
                        data.queue->Push(KeyReleasedEvent(static_cast<KeyCode>(key)));
                    } break;
                    case GLFW_REPEAT: {
                        data.queue->Push(KeyPressedEvent(static_cast<KeyCode>(key), true));
                    } break;
                    default:
                        LOG_WARN("Unknown key action {}", action);
                }
            }
        });

        glfwSetCharCallback(m_Window, [](GLFWwindow* window, u32 codepoint) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                data.queue->Push(KeyTypedEvent(codepoint));
            }
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, i32 button, i32 action, i32) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                switch (action) {
                    case GLFW_PRESS: {
                        data.queue->Push(MouseButtonPressedEvent(static_cast<MouseButton>(button)));
                    } break;
                    case GLFW_RELEASE: {
                        data.queue->Push(MouseButtonReleasedEvent(static_cast<MouseButton>(button)));
                    } break;
                    default:
                        LOG_WARN("Unknown mouse button action {}", action);
                }
            }
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, f64 x, f64 y) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                data.queue->Push(MouseMovedEvent(x, y));
            }
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, f64 x, f64 y) {
            WindowData& data = *reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));

            if (data.queue) {
                data.queue->Push(MouseScrolledEvent(x, y));
            }
        });
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_Window);
        if (s_InstanceCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            glfwTerminate();
        }
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
    }

}
