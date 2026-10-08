#include "Input.hpp"

#include <array>
#include <iostream>

namespace dw4::desktop {

game::InputFrame normalize_buttons(const DeviceButtons& buttons) noexcept
{
    std::uint8_t mask = 0;
    const auto add = [&](const game::Button button, const bool active) {
        if (active) mask |= static_cast<std::uint8_t>(button);
    };
    add(game::Button::a, buttons.a);
    add(game::Button::b, buttons.b);
    add(game::Button::select, buttons.select);
    add(game::Button::start, buttons.start);
    add(game::Button::up, buttons.up || buttons.vertical <= -16000);
    add(game::Button::down, buttons.down || buttons.vertical >= 16000);
    add(game::Button::left, buttons.left || buttons.horizontal <= -16000);
    add(game::Button::right, buttons.right || buttons.horizontal >= 16000);
    return {mask};
}

Input::Input(const SDL_JoystickID preferred)
{
    if (preferred != 0) gamepad_.reset(SDL_OpenGamepad(preferred));
    else open_available();
}

void Input::open_available()
{
    if (gamepad_) return;
    int count = 0;
    const std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)> devices(SDL_GetGamepads(&count), SDL_free);
    for (int index = 0; devices && index < count; ++index) {
        gamepad_.reset(SDL_OpenGamepad(devices.get()[index]));
        if (gamepad_) {
            std::cout << "gamepad connected: " << SDL_GetGamepadName(gamepad_.get()) << '\n';
            return;
        }
    }
}

void Input::handle(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED) {
        SDL_ResetKeyboard();
        focused_ = false;
        wait_for_neutral_ = true;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_RESTORED) focused_ = true;
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) open_available();
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED && gamepad_ && event.gdevice.which == SDL_GetGamepadID(gamepad_.get())) {
        gamepad_.reset();
        open_available();
    }
}

game::InputFrame Input::sample() const noexcept
{
    if (!focused_) return {};
    const auto* keys = SDL_GetKeyboardState(nullptr);
    DeviceButtons keyboard{keys[SDL_SCANCODE_Z], keys[SDL_SCANCODE_X], keys[SDL_SCANCODE_RSHIFT], keys[SDL_SCANCODE_RETURN],
                            keys[SDL_SCANCODE_UP], keys[SDL_SCANCODE_DOWN], keys[SDL_SCANCODE_LEFT], keys[SDL_SCANCODE_RIGHT]};
    auto result = normalize_buttons(keyboard);
    if (gamepad_ && SDL_GamepadConnected(gamepad_.get())) {
        DeviceButtons pad;
        pad.a = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_SOUTH);
        pad.b = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_EAST);
        pad.select = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_BACK);
        pad.start = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_START);
        pad.up = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_DPAD_UP);
        pad.down = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        pad.left = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        pad.right = SDL_GetGamepadButton(gamepad_.get(), SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        pad.horizontal = SDL_GetGamepadAxis(gamepad_.get(), SDL_GAMEPAD_AXIS_LEFTX);
        pad.vertical = SDL_GetGamepadAxis(gamepad_.get(), SDL_GAMEPAD_AXIS_LEFTY);
        result.buttons |= normalize_buttons(pad).buttons;
    }
    if (wait_for_neutral_) {
        if (result.buttons == 0) wait_for_neutral_ = false;
        return {};
    }
    return result;
}

} // namespace dw4::desktop