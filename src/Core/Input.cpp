#include "Input.hpp"

#include "Application.hpp"

bool Input::IsKeyPressed(KeyCode key)
{
    return s_KeyData.find(key) != s_KeyData.end() && s_KeyData[key].state == KeyState::Pressed;
}

bool Input::IsKeyHeld(KeyCode key)
{
    return s_KeyData.find(key) != s_KeyData.end() && s_KeyData[key].state == KeyState::Held;
}

bool Input::IsKeyDown(KeyCode key)
{
    return s_KeyData.find(key) != s_KeyData.end() && (s_KeyData[key].state == KeyState::Pressed || s_KeyData[key].state == KeyState::Held);
}

bool Input::IsKeyReleased(KeyCode key)
{
    return s_KeyData.find(key) != s_KeyData.end() && s_KeyData[key].state == KeyState::Released;
}

bool Input::IsMouseButtonPressed(MouseButton button)
{
    return s_MouseButtonData.find(button) != s_MouseButtonData.end() && s_MouseButtonData[button].state == KeyState::Pressed;
}

bool Input::IsMouseButtonHeld(MouseButton button)
{
    return s_MouseButtonData.find(button) != s_MouseButtonData.end() && s_MouseButtonData[button].state == KeyState::Held;
}

bool Input::IsMouseButtonDown(MouseButton button)
{
    return s_MouseButtonData.find(button) != s_MouseButtonData.end() && (s_MouseButtonData[button].state == KeyState::Pressed || s_MouseButtonData[button].state == KeyState::Held);
}

bool Input::IsMouseButtonReleased(MouseButton button)
{
    return s_MouseButtonData.find(button) != s_MouseButtonData.end() && s_MouseButtonData[button].state == KeyState::Released;
}

f32 Input::GetMouseX()
{
    auto [x, y] = GetMousePosition();
    return static_cast<f32>(x);
}

f32 Input::GetMouseY()
{
    auto [x, y] = GetMousePosition();
    return static_cast<f32>(y);
}

std::pair<f32, f32> Input::GetMousePosition()
{
    return s_MousePos;
}

void Input::Update()
{
    for (auto& [key, data] : s_KeyData) {
        data.oldState = data.state;
        if (data.state == KeyState::Pressed) {
            data.state = KeyState::Held;
        } else if (data.state == KeyState::Released) {
            data.state = KeyState::None;
        }
    }

    for (auto& [button, data] : s_MouseButtonData) {
        data.oldState = data.state;
        if (data.state == KeyState::Pressed) {
            data.state = KeyState::Held;
        } else if (data.state == KeyState::Released) {
            data.state = KeyState::None;
        }
    }
}

void Input::OnEvent(const Event& event)
{
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<KeyPressedEvent>([](const KeyPressedEvent& e) {
        if (!e.repeat) Input::UpdateKeyState(e.keycode, KeyState::Pressed);
        return false;
    });

    dispatcher.Dispatch<KeyReleasedEvent>([](const KeyReleasedEvent& e) {
        Input::UpdateKeyState(e.keycode, KeyState::Released);
        return false;
    });

    dispatcher.Dispatch<MouseButtonPressedEvent>([](const MouseButtonPressedEvent& e) {
        Input::UpdateButtonState(e.button, KeyState::Pressed);
        return false;
    });

    dispatcher.Dispatch<MouseButtonReleasedEvent>([](const MouseButtonReleasedEvent& e) {
        Input::UpdateButtonState(e.button, KeyState::Released);
        return false;
    });

    dispatcher.Dispatch<MouseMovedEvent>([](const MouseMovedEvent& e) {
        Input::UpdateMousePosition(e.x, e.y);
        return false;
    });
}

void Input::UpdateKeyState(KeyCode key, KeyState state)
{
    auto& data = s_KeyData[key];
    data.oldState = data.state;
    data.state = state;
}

void Input::UpdateButtonState(MouseButton button, KeyState state)
{
    auto& data = s_MouseButtonData[button];
    data.oldState = data.state;
    data.state = state;
}

void Input::UpdateMousePosition(f32 x, f32 y)
{
    s_MousePos = std::make_pair(x, y);
}
