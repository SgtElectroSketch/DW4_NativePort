#include <dw4/game/Game.hpp>

namespace dw4::game {

bool InputFrame::is_pressed(const Button button) const noexcept
{
    return (buttons & static_cast<std::uint8_t>(button)) != 0;
}

void Game::tick(const InputFrame input) noexcept
{
    ++frame_.number;
    frame_.input = input;
}

const FrameState& Game::frame() const noexcept
{
    return frame_;
}

} // namespace dw4::game