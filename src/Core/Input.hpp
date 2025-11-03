#pragma once

#include "KeyCodes.hpp"

namespace PathTracer {

    struct KeyData
    {
        KeyState state { KeyState::None };
        KeyState oldState { KeyState::None };
    };

    struct ButtonData
    {
        KeyState state { KeyState::None };
        KeyState oldState { KeyState::None };
    };

    class Input
    {
        friend class Application;
    public:
        static bool IsKeyPressed(KeyCode key);
        static bool IsKeyHeld(KeyCode key);
        static bool IsKeyDown(KeyCode key);
        static bool IsKeyReleased(KeyCode key);

        static bool IsMouseButtonPressed(MouseButton button);
        static bool IsMouseButtonHeld(MouseButton button);
        static bool IsMouseButtonDown(MouseButton button);
        static bool IsMouseButtonReleased(MouseButton button);

        static f32 GetMouseX();
        static f32 GetMouseY();
        static std::pair<f32, f32> GetMousePosition();

    protected:
        static void UpdateKeyState(KeyCode key, KeyState state);
        static void UpdateButtonState(MouseButton button, KeyState state);
        static void UpdateMousePosition(f32 x, f32 y);

        static void Update();

    private:
        inline static std::map<KeyCode, KeyData> s_KeyData;
        inline static std::map<MouseButton, ButtonData> s_MouseButtonData;
        inline static std::atomic<std::pair<f32, f32>> s_MousePos {{ 0.0f, 0.0f }};
    };

}
