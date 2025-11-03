#pragma once

#include "Types.hpp"
#include "KeyCodes.hpp"
#include "Logger.hpp"

namespace PathTracer {

    #define BIT(x) (1 << (x))

    enum EventCategory
    {
        None = 0,
        EventCategoryWindow         = BIT(0),
        EventCategoryInput          = BIT(1),
        EventCategoryKeyboard       = BIT(2),
        EventCategoryMouseButton    = BIT(3),
        EventCategoryMouse          = BIT(4),
    };

    struct BaseEvent { bool handled { false }; };

    struct WindowClosedEvent final : public BaseEvent
    {
        constexpr WindowClosedEvent() noexcept = default;
        static constexpr i32 GetCategoryFlags() { return EventCategoryWindow; }
    };

    struct WindowResizedEvent final : public BaseEvent
    {
        u32 width;
        u32 height;

        constexpr WindowResizedEvent(u32 width, u32 height) noexcept
            : width(width), height(height) {}
        static constexpr i32 GetCategoryFlags() { return EventCategoryWindow; }
    };

    struct WindowMovedEvent final : public BaseEvent
    {
        i32 x;
        i32 y;

        constexpr WindowMovedEvent(i32 x, i32 y) noexcept
            : x(x), y(y) {}
        static constexpr i32 GetCategoryFlags() { return EventCategoryWindow; }
    };

    struct WindowMinimizeEvent final : public BaseEvent
    {
        bool minimized;

        constexpr WindowMinimizeEvent(bool minimized) noexcept
            : minimized(minimized) {}
        static constexpr i32 GetCategoryFlags() { return EventCategoryWindow; }
    };

    struct WindowFocusEvent final : public BaseEvent
    {
        bool focused;

        constexpr WindowFocusEvent(bool focused) noexcept
            : focused(focused) {}
        static constexpr i32 GetCategoryFlags() { return EventCategoryWindow; }
    };

    struct KeyEvent : public BaseEvent
    {
        KeyCode keycode;

        static constexpr i32 GetCategoryFlags() { return EventCategoryInput | EventCategoryKeyboard; }

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
        static constexpr i32 GetCategoryFlags() { return EventCategoryInput | EventCategoryKeyboard; }
    };

    struct MouseButtonEvent : public BaseEvent
    {
        MouseButton button;

        static constexpr i32 GetCategoryFlags() { return EventCategoryInput | EventCategoryMouseButton; }

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

        static constexpr i32 GetCategoryFlags() { return EventCategoryInput | EventCategoryMouse; }

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

    template <typename T>
    concept IsEvent = requires {
        { std::is_base_of_v<BaseEvent, std::remove_cvref_t<T>> };
        { std::is_copy_constructible_v<std::remove_cvref_t<T>> };
        { T::GetCategoryFlags() } -> std::same_as<i32>;
    };

    template <IsEvent... TEvent>
    using EventVariant = std::variant<TEvent...>;

    using Event = EventVariant<
        WindowClosedEvent,
        WindowResizedEvent,
        WindowMovedEvent,
        WindowMinimizeEvent,
        WindowFocusEvent,
        KeyPressedEvent,
        KeyReleasedEvent,
        KeyTypedEvent,
        MouseButtonPressedEvent,
        MouseButtonReleasedEvent,
        MouseMovedEvent,
        MouseScrolledEvent
    >;

    class EventDispatcher
    {
    public:
        constexpr EventDispatcher(Event& event) noexcept
            : m_Event(event) {}

        template <IsEvent T, typename Func>
            requires std::is_invocable_r_v<bool, Func, const T&>
        inline void Dispatch(Func&& func) noexcept
        {
            if (T* event = std::get_if<T>(&m_Event)) {
                if (!event->handled) {
                    event->handled = std::invoke(std::forward<Func>(func), *event);
                }
            }
        }

    private:
        Event& m_Event;
    };

    class EventQueue
    {
    public:
        EventQueue() = default;
        ~EventQueue() = default;

        EventQueue(const EventQueue&) = delete;
        EventQueue& operator=(const EventQueue&) = delete;
        EventQueue(EventQueue&&) = delete;
        EventQueue& operator=(EventQueue&&) = delete;

        template <IsEvent T>
            requires std::is_constructible_v<Event, T&&>
        inline void Push(T&& event)
        {
            auto tail = m_Tail.load(std::memory_order_relaxed);
            auto nextTail = (tail + 1) & (s_QueueSize - 1);

            if (nextTail == m_Head.load(std::memory_order_acquire)) {
                LOG_ERROR("Event queue full");
                return;
            }

            m_Buffer[tail] = std::forward<T>(event);
            m_Tail.store(nextTail, std::memory_order_release);
        }

        inline std::vector<Event> Poll()
        {
            std::vector<Event> polled;
            polled.reserve(s_QueueSize);

            auto head = m_Head.load(std::memory_order_relaxed);
            auto tail = m_Tail.load(std::memory_order_acquire);

            while (head != tail) {
                polled.push_back(std::move(m_Buffer[head]));
                head = (head + 1) & (s_QueueSize - 1);
            }

            m_Head.store(head, std::memory_order_release);

            return polled;
        }

    private:
        inline static constexpr usize s_QueueSize { 256 };

        alignas(64) std::atomic<usize> m_Head { 0 };
        alignas(64) std::atomic<usize> m_Tail { 0 };

        std::array<Event, s_QueueSize> m_Buffer;
    };

}
