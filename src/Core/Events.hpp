#pragma once

#include "KeyCodes.hpp"

struct BaseEvent
{
    mutable bool handled { false };
};

template <typename T>
concept IsEvent = std::is_base_of_v<BaseEvent, std::remove_cvref_t<T>>;

struct WindowClosedEvent final : public BaseEvent
{
    constexpr WindowClosedEvent() noexcept = default;
};

struct WindowResizedEvent final : public BaseEvent
{
    u32 width;
    u32 height;

    constexpr WindowResizedEvent(u32 width, u32 height) noexcept
        : width(width), height(height) {}
};

struct WindowMinimizeEvent final : public BaseEvent
{
    bool minimized;

    constexpr WindowMinimizeEvent(bool minimized) noexcept
        : minimized(minimized) {}
};

struct KeyEvent : public BaseEvent
{
    KeyCode keycode;

protected:
    constexpr KeyEvent(KeyCode keycode) noexcept
        : keycode(keycode) {}
};

struct KeyPressedEvent final : public KeyEvent
{
    bool repeat;

    constexpr KeyPressedEvent(KeyCode keycode, bool repeat) noexcept
        : KeyEvent(keycode), repeat(repeat) {}
};

struct KeyReleasedEvent final : public KeyEvent
{
    constexpr KeyReleasedEvent(KeyCode keycode) noexcept
        : KeyEvent(keycode) {}
};

struct KeyTypedEvent final : public BaseEvent
{
    u32 codepoint;

    constexpr KeyTypedEvent(u32 code) noexcept
        : codepoint(code) {}
};

struct MouseButtonEvent : public BaseEvent
{
    MouseButton button;

protected:
    constexpr MouseButtonEvent(MouseButton button) noexcept
        : button(button) {}
};

struct MouseButtonPressedEvent final : public MouseButtonEvent
{
    constexpr MouseButtonPressedEvent(MouseButton button) noexcept
        : MouseButtonEvent(button) {}
};

struct MouseButtonReleasedEvent final : public MouseButtonEvent
{
    constexpr MouseButtonReleasedEvent(MouseButton button) noexcept
        : MouseButtonEvent(button) {}
};

struct MouseEvent : public BaseEvent
{
    f32 x;
    f32 y;

protected:
    constexpr MouseEvent(f32 x, f32 y) noexcept
        : x(x), y(y) {}
};

struct MouseMovedEvent final : public MouseEvent
{
    constexpr MouseMovedEvent(f32 x, f32 y) noexcept
        : MouseEvent(x, y) {}
};

struct MouseScrolledEvent final : public MouseEvent
{
    constexpr MouseScrolledEvent(f32 x, f32 y) noexcept
        : MouseEvent(x, y) {}
};

template <IsEvent... TEvent>
using EventVariant = std::variant<TEvent...>;

using Event = EventVariant<
    WindowClosedEvent,
    WindowResizedEvent,
    WindowMinimizeEvent,
    KeyPressedEvent,
    KeyReleasedEvent,
    KeyTypedEvent,
    MouseButtonPressedEvent,
    MouseButtonReleasedEvent,
    MouseMovedEvent,
    MouseScrolledEvent
>;

template <typename F, typename T>
concept EventHandler = std::is_invocable_r_v<bool, F, const T&> || std::is_invocable_r_v<void, F, const T&>;

class EventDispatcher
{
public:
    constexpr EventDispatcher(const Event& event) noexcept
        : m_Event(event) {}

    template <IsEvent T, EventHandler<T> Func>
    inline void Dispatch(Func&& func) noexcept
    {
        if (const T* event = std::get_if<T>(&m_Event)) {
            if (!event->handled) {
                if constexpr (std::is_invocable_r_v<bool, Func, const T&>) {
                    event->handled = std::invoke(std::forward<Func>(func), *event);
                } else {
                    std::invoke(std::forward<Func>(func), *event);
                }
            }
        }
    }

private:
    const Event& m_Event;
};
