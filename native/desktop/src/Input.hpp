#pragma once

#include <dw4/game/Game.hpp>

#include <SDL3/SDL.h>

#include <memory>

namespace dw4::desktop {

struct DeviceButtons final {
    bool a{};
    bool b{};
    bool select{};
    bool start{};
    bool up{};
    bool down{};
    bool left{};
    bool right{};
    std::int16_t horizontal{};
    std::int16_t vertical{};
};

[[nodiscard]] game::InputFrame normalize_buttons(const DeviceButtons& buttons) noexcept;

class Input final {
public:
    explicit Input(SDL_JoystickID preferred = 0);
    void handle(const SDL_Event& event);
    [[nodiscard]] game::InputFrame sample() const noexcept;

private:
    using Gamepad = std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>;
    void open_available();
    Gamepad gamepad_{nullptr, SDL_CloseGamepad};
    bool focused_{true};
    mutable bool wait_for_neutral_{};
};

} // namespace dw4::desktop