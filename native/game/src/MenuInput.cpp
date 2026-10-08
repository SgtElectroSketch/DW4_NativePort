#include <dw4/game/MenuInput.hpp>

#include <limits>
#include <stdexcept>

namespace dw4::game {

MenuInput::MenuInput(const MenuRepeat repeat)
    : repeat_(repeat)
{
    if (repeat.delay == 0 || repeat.interval == 0 || repeat.delay > 240 || repeat.interval > 60) {
        throw std::invalid_argument("menu repeat timing must be positive and bounded");
    }
}

MenuActions MenuInput::tick(InputFrame input) noexcept
{
    constexpr auto vertical = static_cast<std::uint8_t>(static_cast<std::uint8_t>(Button::up) | static_cast<std::uint8_t>(Button::down));
    constexpr auto horizontal = static_cast<std::uint8_t>(static_cast<std::uint8_t>(Button::left) | static_cast<std::uint8_t>(Button::right));
    if ((input.buttons & vertical) == vertical) input.buttons &= static_cast<std::uint8_t>(~vertical);
    if ((input.buttons & horizontal) == horizontal) input.buttons &= static_cast<std::uint8_t>(~horizontal);
    const auto pressed = static_cast<std::uint8_t>(input.buttons & ~previous_);
    previous_ = input.buttons;
    MenuActions actions;
    constexpr std::array<Button, 4> directions{Button::up, Button::down, Button::left, Button::right};
    const std::array<bool*, 4> outputs{&actions.up, &actions.down, &actions.left, &actions.right};
    for (std::size_t index = 0; index < directions.size(); ++index) {
        const auto bit = static_cast<std::uint8_t>(directions[index]);
        auto& held = held_frames_[index];
        if ((input.buttons & bit) == 0) {
            held = 0;
            continue;
        }
        if ((pressed & bit) != 0) {
            held = 0;
            *outputs[index] = true;
            continue;
        }
        ++held;
        if (held >= repeat_.delay && (held - repeat_.delay) % repeat_.interval == 0) {
            *outputs[index] = true;
        }
        if (held >= repeat_.delay + repeat_.interval) {
            held = repeat_.delay;
        }
    }
    actions.confirm = (pressed & static_cast<std::uint8_t>(Button::a)) != 0;
    actions.cancel = (pressed & static_cast<std::uint8_t>(Button::b)) != 0;
    return actions;
}

void MenuInput::reset(const InputFrame held) noexcept
{
    previous_ = held.buttons;
    held_frames_.fill(0);
}

} // namespace dw4::game