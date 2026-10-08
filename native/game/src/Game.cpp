#include <dw4/game/Game.hpp>

namespace dw4::game {

bool InputFrame::is_pressed(const Button button) const noexcept
{
    return (buttons & static_cast<std::uint8_t>(button)) != 0;
}

Game::Game(const FrameState initial) noexcept
    : frame_(initial)
{
}

void Game::tick(const InputFrame input) noexcept
{
    const auto previous_buttons = frame_.input.buttons;
    ++frame_.number;
    frame_.pressed.buttons = static_cast<std::uint8_t>(input.buttons & ~previous_buttons);
    frame_.released.buttons = static_cast<std::uint8_t>(previous_buttons & ~input.buttons);
    frame_.input = input;
}

std::uint8_t Game::next_random_byte() noexcept
{
    Random random(frame_.random);
    const auto value = random.next_byte();
    frame_.random = random.state();
    return value;
}

const FrameState& Game::frame() const noexcept
{
    return frame_;
}

} // namespace dw4::game